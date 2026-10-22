#pragma once

/* Internal capture backend contract; not part of the student-facing HAL. */

#include "hal/capture.h"

#include <stdbool.h>

typedef struct {
    bool (*supports)(timer_num_t timer, pin_num_t pin);
    hal_error_t (*init)(timer_num_t timer, pin_num_t pin,
                        capture_edge_t edge,
                        capture_isr_t handler,
                        void *context);
    hal_error_t (*deinit)(timer_num_t timer, pin_num_t pin);
} capture_driver_t;

const capture_driver_t *get_ctimer_capture_driver(void);
