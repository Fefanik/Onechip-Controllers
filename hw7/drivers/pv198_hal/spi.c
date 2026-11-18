#include "hal/spi.h"

#include "hal/drivers/spi.h"

#include <stddef.h>

static const spi_driver_t *bindings[SPI_PORT_COUNT];

static bool port_valid(spi_port_t port)
{
    switch (port) {
    case SPI_PORT_0: case SPI_PORT_1: case SPI_PORT_2: case SPI_PORT_3:
    case SPI_PORT_5: case SPI_PORT_6: case SPI_PORT_7: case SPI_PORT_9:
        return true;
    default:
        return false;
    }
}

static bool format_valid(spi_clock_polarity_t polarity,
                         spi_clock_phase_t phase,
                         spi_bit_order_t order)
{
    return (unsigned)polarity <= SPI_CLOCK_IDLE_HIGH
        && (unsigned)phase <= SPI_CLOCK_PHASE_SECOND_EDGE
        && (unsigned)order <= SPI_LSB_FIRST;
}

static const spi_driver_t *select_master_driver(
    spi_port_t port, const spi_master_config_t *config)
{
    const spi_driver_t *const drivers[] = {get_lpspi_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports_master(port, config)) return drivers[i];
    return NULL;
}

static const spi_driver_t *select_slave_driver(
    spi_port_t port, const spi_slave_config_t *config)
{
    const spi_driver_t *const drivers[] = {get_lpspi_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports_slave(port, config)) return drivers[i];
    return NULL;
}

hal_error_t spi_master_init(spi_port_t port,
                            const spi_master_config_t *config)
{
    if (!port_valid(port) || config == NULL || config->frequency_hz == 0u
        || !format_valid(config->clock_polarity, config->clock_phase,
                         config->bit_order)) return HAL_ERROR_INVALID_ARGUMENT;
    if (bindings[port] != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const spi_driver_t *driver = select_master_driver(port, config);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    hal_error_t status = driver->master_init(port, config);
    if (status == HAL_OK) bindings[port] = driver;
    return status;
}

hal_error_t spi_master_deinit(spi_port_t port)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    hal_error_t status = driver->master_deinit(port);
    if (status == HAL_OK) bindings[port] = NULL;
    return status;
}

hal_error_t spi_master_transfer(spi_port_t port, const uint8_t *tx,
                                uint8_t *rx, size_t count)
{
    if (!port_valid(port) || (count != 0u && tx == NULL && rx == NULL))
        return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    if (count == 0u) return HAL_OK;
    return driver->master_transfer(port, tx, rx, count);
}

hal_error_t spi_slave_init(spi_port_t port,
                           const spi_slave_config_t *config)
{
    if (!port_valid(port) || config == NULL
        || !format_valid(config->clock_polarity, config->clock_phase,
                         config->bit_order)) return HAL_ERROR_INVALID_ARGUMENT;
    if (bindings[port] != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const spi_driver_t *driver = select_slave_driver(port, config);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    hal_error_t status = driver->slave_init(port, config);
    if (status == HAL_OK) bindings[port] = driver;
    return status;
}

hal_error_t spi_slave_deinit(spi_port_t port)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    hal_error_t status = driver->slave_deinit(port);
    if (status == HAL_OK) bindings[port] = NULL;
    return status;
}

hal_error_t spi_slave_transfer(spi_port_t port, const uint8_t *tx,
                               uint8_t *rx, size_t count)
{
    if (!port_valid(port) || count == 0u || (tx == NULL && rx == NULL))
        return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    return driver->slave_transfer(port, tx, rx, count);
}

hal_error_t spi_slave_register_interrupt(spi_port_t port,
                                         spi_slave_isr_t handler,
                                         void *context)
{
    if (!port_valid(port) || handler == NULL)
        return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    return driver->slave_register_interrupt(port, handler, context);
}

hal_error_t spi_slave_unregister_interrupt(spi_port_t port)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    const spi_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    return driver->slave_unregister_interrupt(port);
}
