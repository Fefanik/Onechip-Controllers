#include <stdio.h>
#include <stdint.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "hal.h"
#include <assert.h>

#define LCD_PIN_RS       PIN_4_13
#define LCD_PIN_RW       PIN_4_14
#define LCD_PIN_E        PIN_4_15
#define LCD_PIN_V0       PIN_4_2
#define LCD_PORT         GPIO_PORT_4
#define LCD_DATA_SHIFT   16u
#define LCD_DATA_MASK    ((uint32_t)0xFFu << LCD_DATA_SHIFT)
#define LCD_PIN_MASK     (LCD_DATA_MASK | ((uint32_t)7u << 13))

#define LCD_INIT_POWERUP_MS     50u
#define LCD_DELAY_1_MS           1u
#define LCD_DELAY_100_MS       100u
#define LCD_DELAY_1000_MS     1000u
#define LCD_CONTRAST_MAX_PERCENT 100u
#define LCD_CONTRAST_RAW_PER_PERCENT 650u
#define LCD_RS_TYPE_SHIFT 8u
#define LCD_BYTE_MASK 0xFFu
#define LCD_MESSAGE_TYPE_MASK 0x01u

#define LCD_TIMER               TIMER0
#define LCD_TICK_HZ             1000000u
#define LCD_BUFFER_SIZE         32u
#define LCD_DATA_SETUP_US       2u
#define LCD_E_HIGH_US           2u
#define LCD_CHARACTER_EXEC_US   53u
#define LCD_COMMAND_EXEC_US     53u
#define LCD_CLEAR_HOME_EXEC_US  2200u
#define LCD_FUNCTION_SET_EXEC_US 6000u

#define LCD_INIT_FUNCTION_SET_8BIT 0x30u
#define LCD_FUNCTION_SET_MASK      0xE0u
#define LCD_FUNCTION_SET_VALUE     0x20u
#define LCD_INIT_FUNCTION_SET_2LINE 0x38u
#define LCD_INIT_DISPLAY_OFF       0x08u
#define LCD_INIT_ENTRY_MODE        0x06u
#define LCD_INIT_DISPLAY_ON        0x0Cu
#define LCD_CMD_CLEAR           0x01u
#define LCD_CMD_HOME            0x02u
#define LCD_CMD_CLEAR_HOME_MASK 0xFEu
#define LCD_CMD_SET_DDRAM       0x80u
#define LCD_CMD_DISPLAY_CONTROL 0x08u
#define LCD_ROW1_ADDRESS        0x40u
#define LCD_MAX_ROW             1u
#define LCD_MAX_COLUMN          15u

typedef struct lcd lcd_t;

void ec(hal_error_t status){
	if (status != HAL_OK) {
		printf("HAL Error: %d\r\n", status);
		__builtin_trap();
	}
}

#define ERET(call) do { hal_error_t s_ = (call); if (s_ != HAL_OK) return s_; } while (0)


hal_error_t lcd_init_pins(void){
	ERET(gpio_init_port(LCD_PORT, LCD_PIN_MASK, &(gpio_config_t){GPIO_OUTPUT, GPIO_PULL_NONE, 0}));
	ERET(dac_init(LCD_PIN_V0));
	return HAL_OK;
}

hal_error_t lcd_set_contrast(uint8_t percent){
	assert(percent <= LCD_CONTRAST_MAX_PERCENT);
	ERET(dac_write_raw(LCD_PIN_V0,
		(LCD_CONTRAST_MAX_PERCENT - percent) * LCD_CONTRAST_RAW_PER_PERCENT));
	return HAL_OK;
}

typedef enum { LCD_CMD = 0, LCD_CHAR = 1 } lcd_message_t;

hal_error_t lcd_send_nonblocking(lcd_t *lcd, uint8_t byte, lcd_message_t type);

hal_error_t lcd_send(lcd_t *lcd, uint8_t byte, lcd_message_t type){
	return lcd_send_nonblocking(lcd, byte, type);
}

hal_error_t lcd_print(lcd_t *lcd, const char *text){
	if (lcd == NULL || text == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	for (; *text != '\0'; ++text) {
		ERET(lcd_send(lcd, (uint8_t)*text, LCD_CHAR));
	}

	return HAL_OK;
}

hal_error_t lcd_clear(lcd_t *lcd){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	return lcd_send(lcd, LCD_CMD_CLEAR, LCD_CMD);
}

hal_error_t lcd_home(lcd_t *lcd){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	return lcd_send(lcd, LCD_CMD_HOME, LCD_CMD);
}

hal_error_t lcd_set_cursor(lcd_t *lcd, uint8_t row, uint8_t column){
	if (lcd == NULL || row > LCD_MAX_ROW || column > LCD_MAX_COLUMN)
		return HAL_ERROR_INVALID_ARGUMENT;
	uint8_t address = (uint8_t)((row == 0 ? 0x00u : LCD_ROW1_ADDRESS) + column);
	return lcd_send(lcd, (uint8_t)(LCD_CMD_SET_DDRAM | address), LCD_CMD);
}

hal_error_t lcd_display_control(lcd_t *lcd, bool display, bool cursor, bool blink){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	uint8_t command = (uint8_t)(LCD_CMD_DISPLAY_CONTROL
		| ((uint8_t)display << 2) | ((uint8_t)cursor << 1)
		| (uint8_t)blink);
	return lcd_send(lcd, command, LCD_CMD);
}



hal_error_t lcd_init_timer(void){
	return timer_init(LCD_TIMER, LCD_TICK_HZ);
}

typedef enum lcd_state {
	LCD_IDLE,
	LCD_DATA_SETUP,
	LCD_E_HIGH,
	LCD_CAPTURE,
} lcd_state_t;

struct lcd {
	volatile uint16_t buffer[LCD_BUFFER_SIZE];
	volatile size_t send_pos;
	volatile size_t write_pos;
	volatile size_t to_send;
	volatile lcd_state_t state;
	volatile hal_error_t error;
};

static hal_error_t lcd_wait_idle(lcd_t *lcd){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	while (lcd->to_send != 0) {
		ERET(delay_ms(LCD_DELAY_1_MS));
		if (lcd->error != HAL_OK)
			return lcd->error;
	}
	return HAL_OK;
}

hal_error_t lcd_init(lcd_t *lcd){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	const uint8_t commands[] = {
		LCD_INIT_FUNCTION_SET_8BIT, LCD_INIT_FUNCTION_SET_8BIT,
		LCD_INIT_FUNCTION_SET_8BIT, LCD_INIT_FUNCTION_SET_2LINE,
		LCD_INIT_DISPLAY_OFF, LCD_CMD_CLEAR, LCD_INIT_ENTRY_MODE,
		LCD_INIT_DISPLAY_ON
	};
	ERET(delay_ms(LCD_INIT_POWERUP_MS));
	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
		ERET(lcd_send(lcd, commands[i], LCD_CMD));

	return lcd_wait_idle(lcd);
}

static uint32_t lcd_exec_us(uint16_t item){
	uint8_t byte = (uint8_t)(item & LCD_BYTE_MASK);
	lcd_message_t rs = (lcd_message_t)((item >> LCD_RS_TYPE_SHIFT)
		& LCD_MESSAGE_TYPE_MASK);
	if (rs == LCD_CHAR)                         // character write
		return LCD_CHARACTER_EXEC_US;
	if (byte == LCD_CMD_CLEAR ||
		(byte & LCD_CMD_CLEAR_HOME_MASK) == LCD_CMD_HOME) // clear / home
		return LCD_CLEAR_HOME_EXEC_US;
	if ((byte & LCD_FUNCTION_SET_MASK) == LCD_FUNCTION_SET_VALUE)
		return LCD_FUNCTION_SET_EXEC_US;
	return LCD_COMMAND_EXEC_US;
}

void lcd_timer_isr(timer_num_t, void *lcd);

hal_error_t lcd_machine_step(lcd_t *lcd){
	if (lcd == NULL)
		return HAL_ERROR_INVALID_ARGUMENT;
	if (lcd->state == LCD_IDLE)
		lcd->state = LCD_DATA_SETUP;

	uint32_t wait_us;
	switch (lcd->state) {
		case LCD_DATA_SETUP:
			if (lcd->to_send == 0) {
				lcd->state = LCD_IDLE;
				return HAL_OK;
			}
			{
				uint16_t item = lcd->buffer[lcd->send_pos];
				uint8_t byte = (uint8_t)(item & LCD_BYTE_MASK);
				lcd_message_t type = (lcd_message_t)((item >> LCD_RS_TYPE_SHIFT)
					& LCD_MESSAGE_TYPE_MASK);
				ERET(gpio_write_port_masked(LCD_PORT, LCD_DATA_MASK,
					((uint32_t)byte) << LCD_DATA_SHIFT));
				ERET(gpio_write_pin(LCD_PIN_RS, type == LCD_CHAR));
				ERET(gpio_write_pin(LCD_PIN_RW, 0));
				ERET(gpio_write_pin(LCD_PIN_E, 0));
			}
			wait_us = LCD_DATA_SETUP_US;
			lcd->state = LCD_E_HIGH;
			break;

		case LCD_E_HIGH:
			ERET(gpio_write_pin(LCD_PIN_E, 1));
			wait_us = LCD_E_HIGH_US;
			lcd->state = LCD_CAPTURE;
			break;

		case LCD_CAPTURE:
			ERET(gpio_write_pin(LCD_PIN_E, 0));
			wait_us = lcd_exec_us(lcd->buffer[lcd->send_pos]);
			lcd->send_pos = (lcd->send_pos + 1u) % LCD_BUFFER_SIZE;
			--lcd->to_send;
			lcd->state = LCD_DATA_SETUP;
			break;

		default:
			return HAL_ERROR_INVALID_ARGUMENT;
	}

	return timer_match_start_oneshot(LCD_TIMER, TIMER_MATCH0, wait_us, lcd_timer_isr, lcd);
}

void lcd_timer_isr(timer_num_t, void *lcd){
	if (((lcd_t *)lcd)->error == HAL_OK)
		((lcd_t *)lcd)->error = lcd_machine_step(lcd);
}

hal_error_t lcd_send_nonblocking(lcd_t *lcd, uint8_t byte, lcd_message_t type){
	if (lcd == NULL || (type != LCD_CMD && type != LCD_CHAR))
		return HAL_ERROR_INVALID_ARGUMENT;

	irq_state_t irq_state = critical_enter();
	if (lcd->error != HAL_OK) {
		hal_error_t error = lcd->error;
		critical_exit(irq_state);
		return error;
	}
	if (lcd->to_send >= LCD_BUFFER_SIZE) {
		critical_exit(irq_state);
		return HAL_ERROR_BUSY;
	}

	lcd->buffer[lcd->write_pos] = (uint16_t)(((uint16_t)type << LCD_RS_TYPE_SHIFT) | byte);
	lcd->write_pos = (lcd->write_pos + 1u) % LCD_BUFFER_SIZE;
	++lcd->to_send;

	hal_error_t status = HAL_OK;
	if (lcd->state == LCD_IDLE) {
		status = lcd_machine_step(lcd);
		if (status != HAL_OK)
			lcd->error = status;
	}
	critical_exit(irq_state);
	return status;
}


int main(void){
	BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitBootPeripherals();

	printf("Hello World\r\n");

	lcd_t lcd = {0};
	ec(lcd_init_timer());

	ec(lcd_init_pins());
	ec(lcd_init(&lcd));
	ec(lcd_set_contrast(90));
	ec(lcd_send(&lcd, 'H', LCD_CHAR));
	ec(lcd_wait_idle(&lcd));

	ec(lcd_print(&lcd, "ello world!"));
	ec(lcd_wait_idle(&lcd));

	ec(delay_ms(LCD_DELAY_1000_MS));
	ec(lcd_clear(&lcd));
	ec(lcd_wait_idle(&lcd));
	ec(lcd_display_control(&lcd, 0, 0, 0));
	ec(lcd_wait_idle(&lcd));
	ec(delay_ms(LCD_DELAY_1000_MS));
	ec(lcd_home(&lcd));
	ec(lcd_wait_idle(&lcd));
	ec(lcd_display_control(&lcd, 1, 1, 1));
	ec(lcd_wait_idle(&lcd));

	ec(lcd_clear(&lcd));
	ec(lcd_wait_idle(&lcd));
	assert(lcd_print(&lcd,
		"This string is longer than the buffer and should result in HAL_ERROR_BUSY but the application lives on")
		== HAL_ERROR_BUSY);
	ec(lcd_wait_idle(&lcd));
	ec(delay_ms(LCD_DELAY_1000_MS));

	while (1) {
		ec(lcd_home(&lcd));
		ec(lcd_wait_idle(&lcd));
		for (int x = 0; x <= (int)LCD_MAX_COLUMN; x++) {
			ec(lcd_clear(&lcd));
			ec(lcd_set_cursor(&lcd, 0, (uint8_t)x));
			ec(lcd_send(&lcd, '>', LCD_CHAR));
			ec(lcd_wait_idle(&lcd));
			ec(delay_ms(LCD_DELAY_100_MS));
		}
		for (int x = (int)LCD_MAX_COLUMN; x >= 0; x--) {
			ec(lcd_clear(&lcd));
			ec(lcd_set_cursor(&lcd, 1, (uint8_t)x));
			ec(lcd_send(&lcd, '<', LCD_CHAR));
			ec(lcd_wait_idle(&lcd));
			ec(delay_ms(LCD_DELAY_100_MS));
		}
	}

	return 0;
}
