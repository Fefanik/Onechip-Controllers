#pragma once

/* Internal PWM backend contract; not part of the student-facing HAL. */
#include "hal/pwm.h"
#include <stdbool.h>

/**
 * @brief Configure a backend PWM channel without starting its timer.
 * @param timer Initialized, stopped timer with a finite period.
 * @param pin PWM-capable pin.
 * @param pulse_ticks High time in ticks.
 * @return HAL_OK on success, otherwise a HAL error.
 */
typedef hal_error_t (*pwm_driver_init_t)(timer_num_t timer, pin_num_t pin,
                                        uint32_t pulse_ticks);

/**
 * @brief Release a backend PWM channel, retaining its timebase.
 * @param timer Selected hardware timer.
 * @param pin PWM pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
typedef hal_error_t (*pwm_driver_deinit_t)(timer_num_t timer, pin_num_t pin);

/**
 * @brief Set a backend PWM pulse width.
 * @param timer Selected hardware timer.
 * @param pin Initialized PWM pin.
 * @param pulse_ticks High time in ticks.
 * @return HAL_OK on success, otherwise a HAL error.
 */
typedef hal_error_t (*pwm_driver_set_pulse_t)(timer_num_t timer, pin_num_t pin,
                                            uint32_t pulse_ticks);

/**
 * @brief Drive a backend PWM channel low without releasing it.
 * @param timer Selected hardware timer.
 * @param pin Initialized PWM pin.
 * @return HAL_OK on success, otherwise a HAL error.
 */
typedef hal_error_t (*pwm_driver_stop_t)(timer_num_t timer, pin_num_t pin);

typedef struct {
    bool (*supports)(timer_num_t timer, pin_num_t pin);
    pwm_driver_init_t init;
    pwm_driver_deinit_t deinit;
    pwm_driver_set_pulse_t set_pulse;
    pwm_driver_stop_t stop;
} pwm_driver_t;

/**
 * @brief Get the CTIMER PWM backend.
 * @return Immutable CTIMER driver function table.
 */
const pwm_driver_t *get_ctimer_driver(void);
