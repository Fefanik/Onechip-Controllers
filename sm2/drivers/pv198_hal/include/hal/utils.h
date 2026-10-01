#pragma once

#include "hal/error.h"

#include <stdint.h>

/**
 * @brief Block for at least the requested duration.
 * @param duration_ms Delay in milliseconds.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t delay_ms(uint32_t duration_ms);

/*
 * Disables interrupts and returns their previous state. Calls may be nested if
 * each enter is paired with an exit using its matching state value.
 */
typedef uint32_t irq_state_t;

/**
 * @brief Enter a short interrupt-safe critical section.
 * @return Previous interrupt state, to be passed to critical_exit().
 */
irq_state_t critical_enter(void);

/**
 * @brief Leave a critical section and restore the previous interrupt state.
 * @param previous_state State returned by the matching critical_enter().
 */
void critical_exit(irq_state_t previous_state);
