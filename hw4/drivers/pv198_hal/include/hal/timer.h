#pragma once

#include "hal/error.h"
#include "hal/gpio.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Hardware timers and their hardware pin routes.
 *
 * Match outputs and fixed capture routes (each cell lists alternative pins):
 *
 * | Timer  | MAT0                      | MAT1                      | MAT2                      | MAT3                      | CAP0                     | CAP1                     | CAP2                      | CAP3                      |
 * | ------ | ------------------------- | ------------------------- | ------------------------- | ------------------------- | ------------------------ | ------------------------ | ------------------------- | ------------------------- |
 * | TIMER0 | P0_2, P0_10, P0_16, P0_24 | P0_3, P0_11, P0_17, P0_25 | P0_4, P0_12, P0_18, P0_26 | P0_5, P0_13, P0_19, P0_27 | P0_8, P0_20, P0_28 | P0_9, P0_21, P0_29 | P0_6, P0_14, P0_22, P0_30 | P0_7, P0_15, P0_23, P0_31 |
 * | TIMER1 | P1_2, P3_10               | P1_3, P3_11               | P1_4, P3_12               | P1_5, P3_13               | P1_0, P3_8               | P1_1, P3_9               | P1_6, P3_14               | P1_7, P3_15               |
 * | TIMER2 | P1_10, P3_18, P4_20       | P1_11, P3_19, P4_21       | P1_12, P3_20, P4_22       | P1_13, P3_21, P4_23       | P1_8, P3_16              | P1_9, P3_17              | P1_14, P3_22              | P1_15, P3_23              |
 * | TIMER3 | P1_18, P4_16              | P1_19, P4_17              | P1_20, P4_18              | P1_21, P4_19              | P1_16, P4_2              | P1_17, P4_3              | P1_22, P4_4               | P1_23, P4_5               |
 * | TIMER4 | P3_2, P4_12               | P3_3, P4_13               | P3_6, P4_14               | P3_7, P4_15               | P1_30, P3_0, P4_0        | P1_31, P3_1, P4_1        | P3_4, P4_6                | P3_5, P4_7                |
 *
 * Capture routing is fixed by the HAL: input 4 * timer + channel feeds CAP0-CAP3.
 * Only one alternative pin per capture channel may be initialized at a time.
 * The driver configures the input routing internally.
 * P0_0/P0_1 are reserved for SWD and cannot be used by the HAL.
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

/** Match channels on each hardware timer. */
typedef enum {
    TIMER_MATCH0 = 0, /**< Match channel 0. */
    TIMER_MATCH1,     /**< Match channel 1. */
    TIMER_MATCH2,     /**< Match channel 2. */
    TIMER_MATCH3,     /**< Match channel 3. */
} timer_match_t;

/**
 * @brief Match callback executed in interrupt context.
 * @param timer Timer containing the match channel.
 * @param context User context supplied when registering the callback.
 */
typedef void (*timer_match_isr_t)(timer_num_t timer, void *context);

/** Hardware behavior configured for one match channel. */
typedef struct {
    uint32_t count;       /**< Counter value that triggers the match. */
    bool reset_counter;   /**< Reset the counter at the match. */
    bool stop_timer;      /**< Stop the shared timer at the match. */
} timer_match_config_t;

/**
 * @brief Configure a hardware match channel.
 * @param timer Initialized hardware timer; it must be stopped.
 * @param match Match channel to configure.
 * @param config Match count and timer reset/stop behavior; must not be NULL.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT for invalid inputs,
 *         HAL_ERROR_NOT_INITIALIZED if the timer is not initialized, or
 *         HAL_ERROR_BUSY if the timer is running.
 */
hal_error_t timer_match_set(timer_num_t timer, timer_match_t match,
                            const timer_match_config_t *config);

/**
 * @brief Attach or replace a callback for a match channel.
 * @param timer Initialized hardware timer; it must be stopped.
 * @param match Match channel whose interrupt should be configured.
 * @param handler Callback, or NULL to disable the interrupt.
 * @param context User context passed to handler.
 * @note The callback remains registered until explicitly removed or the timer
 *       is stopped. Hardware reset/stop behavior is unchanged.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, or HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_match_set_interrupt(timer_num_t timer, timer_match_t match,
                                      timer_match_isr_t handler,
                                      void *context);

/**
 * @brief Remove a match callback while retaining channel configuration.
 * @param timer Initialized hardware timer.
 * @param match Match channel whose callback should be removed.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, or HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_match_unset_interrupt(timer_num_t timer, timer_match_t match);

/**
 * @brief Restart the timer and schedule a one-shot match from zero.
 * @param timer Initialized, stopped hardware timer.
 * @param match Match channel to schedule.
 * @param offset_ticks Number of ticks until the match; must be nonzero.
 * @param handler Callback invoked when the match fires; must not be NULL.
 * @param context User context passed to handler.
 * @note Starting resets the shared counter. The match stops the timer and
 *       removes its callback when it fires. Existing counter reset behavior
 *       is retained. PWM/capture channels must be released before scheduling.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, or HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_match_start_oneshot(timer_num_t timer, timer_match_t match,
                                      uint32_t offset_ticks,
                                      timer_match_isr_t handler,
                                      void *context);

/**
 * @brief Start a periodic match by advancing its compare value each time it fires.
 * @param timer Initialized, stopped hardware timer.
 * @param match Match channel to schedule.
 * @param interval_ticks Ticks between matches; must be nonzero.
 * @param handler Callback invoked on each match; must not be NULL.
 * @param context User context passed to handler.
 * @note Starting resets the shared counter and schedules the first match from
 *       zero. Existing hardware reset behavior is retained. PWM/capture
 *       channels must be released before scheduling.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, or HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_match_start_periodic(timer_num_t timer, timer_match_t match,
                                       uint32_t interval_ticks,
                                       timer_match_isr_t handler,
                                       void *context);

/** Edge type used by timer capture. */
typedef gpio_interrupt_t timer_capture_edge_t;

/**
 * @brief Timer capture callback executed in interrupt context.
 * @param pin Pin on which the edge was captured.
 * @param edge Captured edge; for GPIO_ANY_EDGE, the detected edge.
 * @param timestamp_ticks Captured timer counter value in ticks.
 * @param context User context supplied when registering the callback.
 */
typedef void (*timer_capture_isr_t)(pin_num_t pin,
                                    gpio_interrupt_t edge,
                                    uint32_t timestamp_ticks,
                                    void *context);

/**
 * @brief Configure hardware edge capture on a timer capture input.
 * @param timer Initialized, stopped hardware timer.
 * @param pin Timer capture-capable pin routed to the selected timer;
 *            P0_0/P0_1 are reserved for SWD and are not supported.
 * @param edge Rising, falling, or any edge.
 * @param handler Callback invoked for each captured edge; must not be NULL.
 * @param context User context passed to handler.
 * @note An unused pin is configured without a pull resistor. A pin initialized
 *       as GPIO_INPUT may instead be handed over to capture; its pull and other
 *       electrical settings are preserved and only its mux is changed. GPIO
 *       outputs and pins owned by other peripherals cannot be taken over.
 *       Unregister any GPIO interrupts on this pin before handing it over.
 *       After success, GPIO pin initialization/read/write/deinitialization
 *       cannot be used on it until capture is deinitialized.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, HAL_ERROR_NOT_SUPPORTED, or
 *         HAL_ERROR_BUSY on failure; HAL_ERROR_ALREADY_INITIALIZED if the
 *         capture is already initialized or another peripheral owns the pin.
 *         Reserved SWD pins return HAL_ERROR_NOT_SUPPORTED without hardware changes.
 */
hal_error_t timer_capture_init(timer_num_t timer, pin_num_t pin,
                               gpio_interrupt_t edge,
                               timer_capture_isr_t handler,
                               void *context);

/**
 * @brief Disable timer capture and release its input pin.
 * @note The pin is returned to an unclaimed, input-safe state without pulls;
 *       a previous GPIO configuration/ownership is not restored.
 * @param timer Timer containing the capture channel.
 * @param pin Capture pin to release.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_NOT_INITIALIZED, HAL_ERROR_NOT_SUPPORTED, or
 *         HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_capture_deinit(timer_num_t timer, pin_num_t pin);

/**
 * @brief Initialize the hardware counter and its tick rate.
 * @param timer Hardware timer to initialize.
 * @param tick_hz Counter frequency in hertz; the source clock must divide it
 *        exactly.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_ALREADY_INITIALIZED, or HAL_ERROR_NOT_SUPPORTED on failure.
 * @note The counter starts stopped and free-running. Configure match channels
 *       separately with timer_match_set().
 */
hal_error_t timer_init(timer_num_t timer, uint32_t tick_hz);

/**
 * @brief Initialize a timer and configure TIMER_MATCH2 as its period.
 * @param timer Hardware timer to initialize.
 * @param tick_hz Counter frequency in hertz.
 * @param period_ticks Period in ticks, or TIMER_FREE_RUNNING.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT,
 *         HAL_ERROR_ALREADY_INITIALIZED, or HAL_ERROR_NOT_SUPPORTED on failure.
 */
hal_error_t timer_init_period(timer_num_t timer, uint32_t tick_hz,
                              uint32_t period_ticks);

/**
 * @brief Start the initialized counter from zero.
 * @param timer Initialized, stopped hardware timer.
 * @return HAL_OK on success; HAL_ERROR_NOT_INITIALIZED or HAL_ERROR_BUSY on failure.
 */
hal_error_t timer_start(timer_num_t timer);

/**
 * @brief Stop the counter and disable active match callbacks.
 * @param timer Initialized hardware timer.
 * @return HAL_OK on success; HAL_ERROR_NOT_INITIALIZED on failure.
 */
hal_error_t timer_stop(timer_num_t timer);

/**
 * @brief Read the current counter value in timer ticks.
 * @param timer Initialized hardware timer.
 * @param[out] ticks Receives the current counter value; must not be NULL.
 * @return HAL_OK on success; HAL_ERROR_INVALID_ARGUMENT or
 *         HAL_ERROR_NOT_INITIALIZED on failure. On failure, ticks is unchanged.
 */
hal_error_t timer_get_count(timer_num_t timer, uint32_t *ticks);

/**
 * @brief Stop and release the timer and its configured channels.
 * @param timer Initialized hardware timer.
 * @return HAL_OK on success; HAL_ERROR_NOT_INITIALIZED or HAL_ERROR_BUSY if
 *         PWM/capture resources are still attached.
 */
hal_error_t timer_deinit(timer_num_t timer);
