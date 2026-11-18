#pragma once

#include "hal/error.h"
#include "hal/gpio.h"
#include "hal/pinout.h"

#include "fsl_gpio.h"
#include "fsl_port.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HAL_PIN_OWNER_NONE = 0,
    HAL_PIN_OWNER_GPIO,
    HAL_PIN_OWNER_PWM,
    HAL_PIN_OWNER_CAPTURE,
    HAL_PIN_OWNER_ADC,
    HAL_PIN_OWNER_DAC,
    HAL_PIN_OWNER_I2C,
    HAL_PIN_OWNER_SPI,
    HAL_PIN_OWNER_UART,
} hal_pin_owner_t;

typedef enum {
    HAL_CTIMER_OWNER_NONE = 0,
    HAL_CTIMER_OWNER_TIMER,
    HAL_CTIMER_OWNER_CAPTURE,
    HAL_CTIMER_OWNER_PWM,
} hal_ctimer_owner_t;

/* Usable digital pin: excludes analog-only pads and SWD (P0_0/P0_1).
 * Ownership helpers additionally accept named analog-only pads for ADC/DAC. */
bool hal_pin_valid(pin_num_t pin);
GPIO_Type *hal_gpio_base(unsigned port);
PORT_Type *hal_port_base(unsigned port);
hal_error_t hal_pin_claim(pin_num_t pin, hal_pin_owner_t owner);
/* Claim an unused capture pin, or take over a GPIO input without resetting
 * its electrical configuration. Reject GPIO outputs and other owners. */
hal_error_t hal_pin_claim_capture(pin_num_t pin, bool *was_gpio_input);
void hal_pin_release(pin_num_t pin, hal_pin_owner_t owner);
bool hal_pin_owned_by(pin_num_t pin, hal_pin_owner_t owner);
hal_error_t hal_flexcomm_claim(unsigned instance, hal_pin_owner_t owner);
void hal_flexcomm_release(unsigned instance, hal_pin_owner_t owner);
uint32_t hal_flexcomm_enable_clock(unsigned instance);
hal_error_t hal_ctimer_claim(unsigned instance, hal_ctimer_owner_t owner);
void hal_ctimer_release(unsigned instance, hal_ctimer_owner_t owner);
bool hal_ctimer_owned_by(unsigned instance, hal_ctimer_owner_t owner);
/* Shared timer timebase; backends must never initialize/start it themselves. */
hal_error_t hal_timer_require(unsigned timer, uint32_t *period);
bool hal_timer_running(unsigned timer);
void hal_timer_set_period(unsigned timer, uint32_t period);
void hal_match_ctimer_irq(unsigned timer);
void hal_match_stop(unsigned timer);
void hal_match_deinit(unsigned timer);
void hal_capture_ctimer_irq(unsigned timer);
void hal_stdio_init(void);
void hal_pin_configure(pin_num_t pin,
                       port_mux_t mux,
                       gpio_pull_t pull,
                       bool open_drain,
                       bool input_enable);
void hal_pin_make_safe(pin_num_t pin);
