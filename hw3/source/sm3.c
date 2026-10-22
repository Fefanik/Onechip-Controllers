#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"

#define ADC_RAW_MAX              65535.0f
#define TEMPERATURE_MIN_C        19.0f
#define TEMPERATURE_MAX_C        23.0f
#define SETPOINT_C               21.0f
#define HYSTERESIS_WIDTH_C       2.0f
#define HEATER_TURN_ON_BELOW_C   (SETPOINT_C - HYSTERESIS_WIDTH_C / 2.0f)
#define HEATER_TURN_OFF_AT_C     (SETPOINT_C + HYSTERESIS_WIDTH_C / 2.0f)

void init(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    gpio_init_pin(PIN_LED_RED, &(gpio_config_t){
        .direction = GPIO_OUTPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 1
    });

    adc_init(PIN_4_2);
}

static float convert_raw_sample_to_temperature_c(uint16_t raw) {
    return TEMPERATURE_MIN_C
            + ((float)raw / ADC_RAW_MAX)
            * (TEMPERATURE_MAX_C - TEMPERATURE_MIN_C);
}



int main(void) {


	init();
    bool heater = false;

    while (1) {
        uint16_t raw;

        adc_read_raw(PIN_4_2, &raw);
        const float temperature_c = convert_raw_sample_to_temperature_c(raw);

        if (temperature_c < HEATER_TURN_ON_BELOW_C) {
            heater = true;
        } else if (temperature_c >= HEATER_TURN_OFF_AT_C) {
            heater = false;
        }

        gpio_write_pin(PIN_LED_RED, heater ? 0 : 1);


        const uint32_t temperature_display_hundredths =
                (uint32_t)(temperature_c * 100.0f + 0.5f);

        printf("raw=%u temperature=%lu.%02lu C heater=%s\r\n",
               (unsigned int)raw,
               (unsigned long)(temperature_display_hundredths / 100UL),
               (unsigned long)(temperature_display_hundredths % 100UL),
			   heater ? "ON" : "OFF");

        __asm volatile ("nop");
    }
    return 0;
}
