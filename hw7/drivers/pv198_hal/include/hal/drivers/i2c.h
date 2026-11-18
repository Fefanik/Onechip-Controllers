#pragma once

/* Internal I2C backend contract; not part of the student-facing HAL. */

#include "hal/i2c.h"

#include <stdbool.h>

typedef struct {
    bool (*supports)(i2c_port_t port, const i2c_config_t *config);
    hal_error_t (*init)(i2c_port_t port, const i2c_config_t *config);
    hal_error_t (*deinit)(i2c_port_t port);
    hal_error_t (*read)(i2c_port_t port, uint8_t address, uint8_t *buffer,
                        size_t count);
    hal_error_t (*write)(i2c_port_t port, uint8_t address,
                         const uint8_t *buffer, size_t count);
    hal_error_t (*write_read)(i2c_port_t port, uint8_t address,
                              const uint8_t *write_buffer, size_t write_count,
                              uint8_t *read_buffer, size_t read_count);
} i2c_driver_t;

const i2c_driver_t *get_lpi2c_driver(void);
