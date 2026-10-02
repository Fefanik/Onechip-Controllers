#pragma once

/* Internal ADC backend contract; not part of the student-facing HAL. */

#include "hal/adc.h"

#include <stdbool.h>

typedef struct {
    bool (*supports)(pin_num_t pin);
    hal_error_t (*init)(pin_num_t pin);
    hal_error_t (*deinit)(pin_num_t pin);
    hal_error_t (*read_raw)(pin_num_t pin, uint16_t *sample);
    hal_error_t (*start_conversion)(pin_num_t pin,
                                    adc_isr_t handler,
                                    void *context);
} adc_driver_t;

const adc_driver_t *get_lpadc_driver(void);
