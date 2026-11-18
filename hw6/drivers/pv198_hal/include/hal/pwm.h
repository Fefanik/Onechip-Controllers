#pragma once

#include "hal/error.h"
#include "hal/pinout.h"
#include "hal/timer.h"
#include <stdint.h>

/**
 * @brief Configure a PWM output on an initialized, stopped timer.
 * @param timer Initialized, stopped timer with a finite period.
 * @param pin MAT0, MAT1 or MAT3 output pin on this timer; MAT2 is reserved for the period.
 * @param pulse_ticks High time in ticks, from zero through the timer period.
 * @note A running timer returns HAL_ERROR_BUSY and is left running; this function
 *       does not stop/start/reset it. Use timer_init(), then pwm_init() for all
 *       outputs, then timer_start(). All channels share its rate and period.
 *       Zero/full-period widths drive constant low/high. Other widths go high
 *       near the end of each cycle. Channel aliases return HAL_ERROR_BUSY.
 * @return HAL_OK on success; HAL_ERROR_BUSY if the timer is running or resources
 *         conflict (including other channel modes); otherwise a HAL error.
 */
hal_error_t pwm_init(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks);

/**
 * @brief Release a PWM channel and its pin, leaving the timer initialized.
 * @param timer Hardware timer that owns the channel.
 * @param pin PWM pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t pwm_deinit(timer_num_t timer, pin_num_t pin);

/**
 * @brief Change a PWM output's high time without restarting the shared counter.
 * @param timer Hardware timer that owns the channel.
 * @param pin Initialized PWM pin.
 * @param pulse_ticks High time in ticks, from zero through the timer period.
 * @note Updates take effect immediately; the current pulse may be shortened or extended.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t pwm_set_pulse_width(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks);

/**
 * @brief Drive a PWM output low immediately, retaining its resources.
 * @param timer Hardware timer that owns the channel.
 * @param pin Initialized PWM pin.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t pwm_stop(timer_num_t timer, pin_num_t pin);
