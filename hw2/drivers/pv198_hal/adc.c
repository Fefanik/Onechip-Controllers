#include "hal/adc.h"

#include "hal/drivers/adc.h"

#include <stddef.h>

typedef struct {
    pin_num_t pin;
    const adc_driver_t *driver;
    adc_isr_t handler;
    void *context;
} adc_binding_t;

static adc_binding_t bindings[8];

static const adc_driver_t *select_driver(pin_num_t pin)
{
    const adc_driver_t *const drivers[] = {get_lpadc_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports(pin)) return drivers[i];
    return NULL;
}

static adc_binding_t *find_binding(pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver != NULL && bindings[i].pin == pin) return &bindings[i];
    return NULL;
}

hal_error_t adc_init(pin_num_t pin)
{
    if (find_binding(pin) != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const adc_driver_t *driver = select_driver(pin);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    adc_binding_t *slot = NULL;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver == NULL) { slot = &bindings[i]; break; }
    if (slot == NULL) return HAL_ERROR_BUSY;
    hal_error_t status = driver->init(pin);
    if (status == HAL_OK) {
        slot->pin = pin;
        slot->driver = driver;
        slot->handler = NULL;
        slot->context = NULL;
    }
    return status;
}

hal_error_t adc_deinit(pin_num_t pin)
{
    adc_binding_t *binding = find_binding(pin);
    if (binding == NULL) return select_driver(pin) == NULL ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    hal_error_t status = binding->driver->deinit(pin);
    if (status == HAL_OK) {
        binding->driver = NULL;
        binding->handler = NULL;
        binding->context = NULL;
    }
    return status;
}

hal_error_t adc_read_raw(pin_num_t pin, uint16_t *sample)
{
    if (sample == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    adc_binding_t *binding = find_binding(pin);
    if (binding == NULL) return select_driver(pin) == NULL ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    return binding->driver->read_raw(pin, sample);
}

hal_error_t adc_read_mv(pin_num_t pin, uint32_t *millivolts)
{
    if (millivolts == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    uint16_t sample;
    hal_error_t status = adc_read_raw(pin, &sample);
    if (status == HAL_OK) *millivolts = ((uint32_t)sample * 3300u + 32767u) / 65535u;
    return status;
}

hal_error_t adc_register_interrupt(pin_num_t pin,
                                   adc_isr_t handler,
                                   void *context)
{
    if (handler == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    adc_binding_t *binding = find_binding(pin);
    if (binding == NULL)
        return select_driver(pin) == NULL
             ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    if (binding->handler != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    binding->context = context;
    binding->handler = handler;
    return HAL_OK;
}

hal_error_t adc_start_conversion(pin_num_t pin)
{
    adc_binding_t *binding = find_binding(pin);
    if (binding == NULL)
        return select_driver(pin) == NULL
             ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    if (binding->handler == NULL) return HAL_ERROR_NOT_INITIALIZED;
    return binding->driver->start_conversion(pin, binding->handler,
                                              binding->context);
}

hal_error_t adc_unregister_interrupt(pin_num_t pin)
{
    adc_binding_t *binding = find_binding(pin);
    if (binding == NULL)
        return select_driver(pin) == NULL
             ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    if (binding->handler == NULL) return HAL_ERROR_NOT_INITIALIZED;
    binding->handler = NULL;
    binding->context = NULL;
    return HAL_OK;
}
