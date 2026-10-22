#include "hal/i2c.h"

#include "hal/drivers/i2c.h"

#include <stddef.h>

static const i2c_driver_t *bindings[I2C_PORT_COUNT];

static bool port_valid(i2c_port_t port)
{
    switch (port) {
    case I2C_PORT_0: case I2C_PORT_1: case I2C_PORT_2: case I2C_PORT_3:
    case I2C_PORT_5: case I2C_PORT_6: case I2C_PORT_7: case I2C_PORT_9:
        return true;
    default:
        return false;
    }
}

static const i2c_driver_t *select_driver(i2c_port_t port,
                                         const i2c_config_t *config)
{
    const i2c_driver_t *const drivers[] = {get_lpi2c_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports(port, config)) return drivers[i];
    return NULL;
}

static bool args_valid(uint8_t address, const void *buffer, size_t count)
{
    return address >= 0x08u && address <= 0x77u
        && (count == 0u || buffer != NULL);
}

hal_error_t i2c_init(i2c_port_t port, const i2c_config_t *config)
{
    if (!port_valid(port) || config == NULL || config->frequency_hz == 0u)
        return HAL_ERROR_INVALID_ARGUMENT;
    if (bindings[port] != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const i2c_driver_t *driver = select_driver(port, config);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    hal_error_t status = driver->init(port, config);
    if (status == HAL_OK) bindings[port] = driver;
    return status;
}

hal_error_t i2c_deinit(i2c_port_t port)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    const i2c_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    hal_error_t status = driver->deinit(port);
    if (status == HAL_OK) bindings[port] = NULL;
    return status;
}

hal_error_t i2c_read(i2c_port_t port, uint8_t address, uint8_t *buffer,
                     size_t count)
{
    if (!port_valid(port) || !args_valid(address, buffer, count))
        return HAL_ERROR_INVALID_ARGUMENT;
    const i2c_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    if (count == 0u) return HAL_OK;
    return driver->read(port, address, buffer, count);
}

hal_error_t i2c_write(i2c_port_t port, uint8_t address,
                      const uint8_t *buffer, size_t count)
{
    if (!port_valid(port) || !args_valid(address, buffer, count))
        return HAL_ERROR_INVALID_ARGUMENT;
    const i2c_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    if (count == 0u) return HAL_OK;
    return driver->write(port, address, buffer, count);
}

hal_error_t i2c_write_read(i2c_port_t port, uint8_t address,
                           const uint8_t *write_buffer, size_t write_count,
                           uint8_t *read_buffer, size_t read_count)
{
    if (!port_valid(port) || !args_valid(address, write_buffer, write_count)
        || !args_valid(address, read_buffer, read_count)
        || write_count == 0u || read_count == 0u)
        return HAL_ERROR_INVALID_ARGUMENT;
    const i2c_driver_t *driver = bindings[port];
    if (driver == NULL) return HAL_ERROR_NOT_INITIALIZED;
    return driver->write_read(port, address, write_buffer, write_count,
                              read_buffer, read_count);
}
