#pragma once

/* Internal DAC backend contract; not part of the student-facing HAL. */

#include "hal/dac.h"

#include <stdbool.h>

typedef struct {
    bool (*supports)(pin_num_t pin);
    hal_error_t (*init)(pin_num_t pin);
    hal_error_t (*deinit)(pin_num_t pin);
    hal_error_t (*write_raw)(pin_num_t pin, uint16_t value);
} dac_driver_t;

const dac_driver_t *get_lpdac_driver(void);
