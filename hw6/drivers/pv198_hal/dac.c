#include "hal/dac.h"

#include "hal/drivers/dac.h"

#include <stddef.h>

typedef struct {
    pin_num_t pin;
    const dac_driver_t *driver;
} dac_binding_t;

static dac_binding_t bindings[2];

static const dac_driver_t *select_driver(pin_num_t pin)
{
    const dac_driver_t *const drivers[] = {get_lpdac_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports(pin)) return drivers[i];
    return NULL;
}

static dac_binding_t *find_binding(pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver != NULL && bindings[i].pin == pin)
            return &bindings[i];
    return NULL;
}

static hal_error_t unbound_error(pin_num_t pin)
{
    return select_driver(pin) == NULL
         ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
}

hal_error_t dac_init(pin_num_t pin)
{
    if (find_binding(pin) != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const dac_driver_t *driver = select_driver(pin);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;

    dac_binding_t *slot = NULL;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver == NULL) { slot = &bindings[i]; break; }
    if (slot == NULL) return HAL_ERROR_BUSY;

    hal_error_t status = driver->init(pin);
    if (status == HAL_OK) {
        slot->pin = pin;
        slot->driver = driver;
    }
    return status;
}

hal_error_t dac_deinit(pin_num_t pin)
{
    dac_binding_t *binding = find_binding(pin);
    if (binding == NULL) return unbound_error(pin);
    hal_error_t status = binding->driver->deinit(pin);
    if (status == HAL_OK) binding->driver = NULL;
    return status;
}

hal_error_t dac_write_raw(pin_num_t pin, uint16_t value)
{
    dac_binding_t *binding = find_binding(pin);
    if (binding == NULL) return unbound_error(pin);
    return binding->driver->write_raw(pin, value);
}

hal_error_t dac_write_mv(pin_num_t pin, uint32_t millivolts)
{
    if (millivolts > 3300u) return HAL_ERROR_INVALID_ARGUMENT;
    const uint16_t value = (uint16_t)((millivolts * 65535u + 1650u)
                                      / 3300u);
    return dac_write_raw(pin, value);
}
