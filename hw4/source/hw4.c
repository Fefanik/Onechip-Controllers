#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "hal/gpio.h"
#include "hal/adc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BUTTON_PORT ((gpio_port_t)PIN_PORT(PIN_BUTTON_SW2))
#define BUTTON_MASK (1u << PIN_INDEX(PIN_BUTTON_SW2))
#define POT_PIN PIN_4_2

typedef enum { PENDING, RUNNING, STOPPED, FINISHED } state_t;

typedef struct {
    volatile state_t state;
    volatile uint64_t sum;
    volatile uint32_t count;
} ctx_t;

void adc_handler(pin_num_t pin, uint16_t sample, void *context) {
    (void)pin;
    ctx_t *ctx = context;

    ctx->sum += sample;
    ++ctx->count;

    if (ctx->state == RUNNING) {
        if (adc_start_conversion(POT_PIN) != HAL_OK) {
            ctx->count = 0;
            ctx->sum = 0;
            ctx->state = PENDING;
        }
    } else if (ctx->state == STOPPED) {
        ctx->state = FINISHED;
    }
}

void button_handler(gpio_port_t port, uint32_t pins, void *context) {
    (void)port;
    ctx_t *ctx = context;
    bool level;

    if (!(pins & BUTTON_MASK)){
    	return;
    };
    if (gpio_read_pin(PIN_BUTTON_SW2, &level) != HAL_OK){
    	return;
    }

    if (!level) {
        if (ctx->state != PENDING) return;
        ctx->sum = 0;
        ctx->count = 0;
        ctx->state = RUNNING;
        if (adc_start_conversion(POT_PIN) != HAL_OK)
            ctx->state = PENDING;
    } else if (ctx->state == RUNNING) {
        ctx->state = STOPPED;
    }
}

hal_error_t setup_adc(ctx_t *ctx) {
    hal_error_t err = adc_init(POT_PIN);
    if (err != HAL_OK){
    	return err;
    }
    return adc_register_interrupt(POT_PIN, adc_handler, ctx);
}

hal_error_t setup_button(ctx_t *ctx) {
    gpio_config_t cfg = { .direction = GPIO_INPUT, .pull = GPIO_PULL_UP };
    hal_error_t err = gpio_init_pin(PIN_BUTTON_SW2, &cfg);
    if (err != HAL_OK){
    	return err;
    }

    return gpio_register_port_interrupt(BUTTON_PORT, BUTTON_MASK,
                                        GPIO_ANY_EDGE, button_handler, ctx);
}

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    ctx_t ctx = { .state = PENDING, .sum = 0, .count = 0 };
    hal_error_t err = setup_adc(&ctx);

    if (err == HAL_OK) {
    	err = setup_button(&ctx);
    }
    else if (err != HAL_OK) {
        return 1;
    }

    while (true) {
        if (ctx.state == FINISHED) {
            uint32_t average = (uint32_t)(ctx.sum / ctx.count);
            ctx.state = PENDING;
            printf("Average ADC value: %lu\r\n", (unsigned long)average);
        }
    }
}
