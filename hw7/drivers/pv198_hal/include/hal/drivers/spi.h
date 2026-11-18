#pragma once

/* Internal SPI backend contract; not part of the student-facing HAL. */

#include "hal/spi.h"

#include <stdbool.h>

typedef struct {
    bool (*supports_master)(spi_port_t port,
                            const spi_master_config_t *config);
    bool (*supports_slave)(spi_port_t port,
                           const spi_slave_config_t *config);
    hal_error_t (*master_init)(spi_port_t port,
                               const spi_master_config_t *config);
    hal_error_t (*master_deinit)(spi_port_t port);
    hal_error_t (*master_transfer)(spi_port_t port, const uint8_t *tx,
                                   uint8_t *rx, size_t count);
    hal_error_t (*slave_init)(spi_port_t port,
                              const spi_slave_config_t *config);
    hal_error_t (*slave_deinit)(spi_port_t port);
    hal_error_t (*slave_transfer)(spi_port_t port, const uint8_t *tx,
                                  uint8_t *rx, size_t count);
    hal_error_t (*slave_register_interrupt)(spi_port_t port,
                                            spi_slave_isr_t handler,
                                            void *context);
    hal_error_t (*slave_unregister_interrupt)(spi_port_t port);
} spi_driver_t;

const spi_driver_t *get_lpspi_driver(void);
