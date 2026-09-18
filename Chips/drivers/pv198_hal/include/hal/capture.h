#pragma once

#include "hal/error.h"
#include "hal/pinout.h"
#include "hal/timer.h"

#include <stdint.h>

typedef enum {
    CAPTURE_RISING = 0,
    CAPTURE_FALLING,
    CAPTURE_ANY,
} capture_edge_t;

/**
 * @brief Input-capture callback; executes in interrupt context.
 * @param pin Pin on which the edge was captured.
 * @param edge Edge that was captured.
 * @param timestamp_ticks Captured counter value in timer ticks (not elapsed time).
 * @note Timestamps wrap at the configured period, or at 2^32 ticks when free-running.
 *       Starting the timer resets the timestamp origin. CAPTURE_ANY infers edge polarity from the input level in the ISR.
 *       Edges arriving faster than interrupt servicing can be lost or mislabelled.
 * @param context User context supplied during initialization.
 */
typedef void (*capture_isr_t)(pin_num_t pin,
                              capture_edge_t edge,
                              uint32_t timestamp_ticks,
                              void *context);

/**
 * @brief Configure hardware-timed edge capture on a CT_INP-capable pin.
 * @param timer Initialized, stopped timer supplying the shared tick rate and period.
 * @param pin Input pin on the selected timer, as listed in timer.h.
 * @note A running timer returns HAL_ERROR_BUSY and is left running; this function
 *       does not stop/start/reset it. Use timer_init(), then capture_init() for all
 *       inputs, then timer_start(). Each input has a fixed timer and capture channel. A mismatched timer
 *       returns HAL_ERROR_NOT_SUPPORTED; an occupied channel returns HAL_ERROR_BUSY.
 * @param edge Edge or edges to capture.
 * @param handler Callback invoked for each captured edge.
 * @param context User context passed to the callback.
 * @return HAL_OK on success; HAL_ERROR_BUSY if the timer is running or resources
 *         conflict (including other channel modes or occupied capture inputs);
 *         otherwise a HAL error.
 */
hal_error_t capture_init(timer_num_t timer,
                         pin_num_t pin,
                         capture_edge_t edge,
                         capture_isr_t handler,
                         void *context);

/**
 * @brief Disable capture and release its pin, leaving the timebase initialized.
 * @param timer Hardware timer used during initialization.
 * @param pin Capture pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t capture_deinit(timer_num_t timer, pin_num_t pin);
