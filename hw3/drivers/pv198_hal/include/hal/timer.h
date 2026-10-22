#pragma once

#include "hal/error.h"

#include <stdint.h>

/**
 * @brief Hardware timers and their hardware pin routes.
 *
 * Match outputs and fixed capture routes (each cell lists alternative pins):
 *
 * | Timer  | MAT0                      | MAT1                      | MAT2                      | MAT3                      | CAP0                     | CAP1                     | CAP2                      | CAP3                      |
 * | ------ | ------------------------- | ------------------------- | ------------------------- | ------------------------- | ------------------------ | ------------------------ | ------------------------- | ------------------------- |
 * | TIMER0 | P0_2, P0_10, P0_16, P0_24 | P0_3, P0_11, P0_17, P0_25 | P0_4, P0_12, P0_18, P0_26 | P0_5, P0_13, P0_19, P0_27 | P0_0, P0_8, P0_20, P0_28 | P0_1, P0_9, P0_21, P0_29 | P0_6, P0_14, P0_22, P0_30 | P0_7, P0_15, P0_23, P0_31 |
 * | TIMER1 | P1_2, P3_10               | P1_3, P3_11               | P1_4, P3_12               | P1_5, P3_13               | P1_0, P3_8               | P1_1, P3_9               | P1_6, P3_14               | P1_7, P3_15               |
 * | TIMER2 | P1_10, P3_18, P4_20       | P1_11, P3_19, P4_21       | P1_12, P3_20, P4_22       | P1_13, P3_21, P4_23       | P1_8, P3_16              | P1_9, P3_17              | P1_14, P3_22              | P1_15, P3_23              |
 * | TIMER3 | P1_18, P4_16              | P1_19, P4_17              | P1_20, P4_18              | P1_21, P4_19              | P1_16, P4_2              | P1_17, P4_3              | P1_22, P4_4               | P1_23, P4_5               |
 * | TIMER4 | P3_2, P4_12               | P3_3, P4_13               | P3_6, P4_14               | P3_7, P4_15               | P1_30, P3_0, P4_0        | P1_31, P3_1, P4_1        | P3_4, P4_6                | P3_5, P4_7                |
 *
 * Capture routing is fixed by the HAL: input 4 * timer + channel feeds CAP0-CAP3.
 * Only one alternative pin per capture channel may be initialized at a time.
 * The driver configures the input routing internally.
 *
 * @note PWM reserves MAT2 on every timer for the period;
 * only MAT0, MAT1 and MAT3 may drive PWM. Capture supports a pin subset.
 * See SUPPORTED_ROUTES.md for implemented routes and
 * pinout.h for board headers and solder-jumper connections. Some pins serve
 * on-board peripherals. PWM outputs sharing a timer share a period;
 * pulse widths are independent. Initialize the shared timebase before configuring
 * channels. Callback, PWM and capture modes remain mutually exclusive on a timer.
 */
typedef enum {
    TIMER0 = 0, /**< TIMER0; match/capture pin tables in timer_num_t. */
    TIMER1,     /**< TIMER1; match/capture pin tables in timer_num_t. */
    TIMER2,     /**< TIMER2; match/capture pin tables in timer_num_t. */
    TIMER3,     /**< TIMER3; match/capture pin tables in timer_num_t. */
    TIMER4,     /**< TIMER4; match/capture pin tables in timer_num_t. */
} timer_num_t;

/** No periodic reset: the counter wraps naturally after 2^32 ticks. */
#define TIMER_FREE_RUNNING 0u

/**
 * @brief Timer expiration callback; executes in interrupt context.
 * @param timer Timer that expired.
 * @param context User context supplied when starting the timer.
 */
typedef void (*timer_isr_t)(timer_num_t timer, void *context);

/**
 * @brief Initialize a shared timebase, stopped with its counter at zero.
 * @param timer Hardware timer (TIMER0–TIMER4); must not already be initialized.
 * @param tick_hz Nonzero counter frequency; an inexact divider returns HAL_ERROR_NOT_SUPPORTED.
 * @param period_ticks Cycle length in ticks (at least 2), or TIMER_FREE_RUNNING.
 * @note All channels use this rate and period. To change them, release channels
 *       and deinitialize the timer first. Periodic operation reserves MAT2 on every timer. Initialization does not select a channel mode.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_init(timer_num_t timer, uint32_t tick_hz, uint32_t period_ticks);

/**
 * @brief Start the shared counter from zero without a timer callback.
 * @param timer Initialized, stopped timer; a running timer returns HAL_ERROR_BUSY.
 * @note Starts all configured channels. Discards pending events from before start.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_start(timer_num_t timer);

/**
 * @brief Read the current hardware counter without changing timer state.
 * @param timer Initialized timer; may be running or stopped, in any channel mode.
 * @param ticks Output counter in ticks at the rate passed to timer_init(); must not be NULL.
 * @note A stopped timer returns its frozen count. Finite periods reset the count
 *       each period; TIMER_FREE_RUNNING wraps at 2^32 ticks. Starting resets to zero.
 *       Unsigned subtraction measures elapsed ticks across a free-running wrap,
 *       provided less than 2^32 ticks elapse and the timer is not restarted.
 * @return HAL_OK, HAL_ERROR_INVALID_ARGUMENT, or HAL_ERROR_NOT_INITIALIZED.
 *         On error, the output is unchanged.
 */
hal_error_t timer_get_count(timer_num_t timer, uint32_t *ticks);

/**
 * @brief Stop and release the shared timebase.
 * @param timer Timer to release.
 * @note Returns HAL_ERROR_BUSY until all PWM/capture channels are released.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_deinit(timer_num_t timer);

/**
 * @brief Start from zero and stop after one configured period, invoking a callback.
 * @param timer Initialized, stopped timer with a finite period and no PWM/capture channels.
 * @param handler Callback invoked on expiration.
 * @param context User context passed to the callback.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_start_oneshot(timer_num_t timer,
                                timer_isr_t handler,
                                void *context);

/**
 * @brief Start from zero and invoke a callback every configured period.
 * @param timer Initialized, stopped timer with a finite period and no PWM/capture channels.
 * @param handler Callback invoked on each expiration.
 * @param context User context passed to the callback.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_start_periodic(timer_num_t timer,
                                 timer_isr_t handler,
                                 void *context);

/**
 * @brief Freeze the shared counter and disable its callbacks without releasing channels.
 * @param timer Timer to stop.
 * @note PWM outputs retain their current level; use pwm_stop to drive one low.
 *       The next start begins at zero, not at the frozen count.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t timer_stop(timer_num_t timer);
