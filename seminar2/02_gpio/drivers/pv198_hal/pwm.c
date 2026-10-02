#include "hal/pwm.h"

#include "hal/drivers/pwm.h"

#include <stddef.h>

typedef struct { timer_num_t timer; pin_num_t pin; const pwm_driver_t *driver; } pwm_binding_t;
static pwm_binding_t bindings[15];

static const pwm_driver_t *select_driver(timer_num_t timer, pin_num_t pin)
{
    const pwm_driver_t *const drivers[] = {get_ctimer_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports(timer, pin)) return drivers[i];
    return NULL;
}

static pwm_binding_t *find_binding(timer_num_t timer, pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver != NULL && bindings[i].timer == timer && bindings[i].pin == pin) return &bindings[i];
    return NULL;
}

hal_error_t pwm_init(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks)
{
    if ((unsigned)timer > TIMER4) return HAL_ERROR_INVALID_ARGUMENT;
    if ((unsigned)pin >= PIN_ENCODED_LIMIT) return HAL_ERROR_INVALID_ARGUMENT;
    if (find_binding(timer, pin) != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    const pwm_driver_t *driver = select_driver(timer, pin);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    pwm_binding_t *slot = NULL;
    for (unsigned i = 0; i < sizeof(bindings) / sizeof(bindings[0]); ++i)
        if (bindings[i].driver == NULL) { slot = &bindings[i]; break; }
    if (slot == NULL) return HAL_ERROR_BUSY;
    hal_error_t status = driver->init(timer, pin, pulse_ticks);
    if (status == HAL_OK) { slot->timer = timer; slot->pin = pin; slot->driver = driver; }
    return status;
}

hal_error_t pwm_deinit(timer_num_t timer, pin_num_t pin)
{
    if ((unsigned)timer > TIMER4) return HAL_ERROR_INVALID_ARGUMENT;
    pwm_binding_t *binding = find_binding(timer, pin);
    if (binding == NULL) return select_driver(timer, pin) == NULL ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    hal_error_t status = binding->driver->deinit(timer, pin);
    if (status == HAL_OK) binding->driver = NULL;
    return status;
}

hal_error_t pwm_set_pulse_width(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks)
{
    if ((unsigned)timer > TIMER4) return HAL_ERROR_INVALID_ARGUMENT;
    pwm_binding_t *binding = find_binding(timer, pin);
    if (binding == NULL) return select_driver(timer, pin) == NULL ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    return binding->driver->set_pulse(timer, pin, pulse_ticks);
}

hal_error_t pwm_stop(timer_num_t timer, pin_num_t pin)
{
    if ((unsigned)timer > TIMER4) return HAL_ERROR_INVALID_ARGUMENT;
    pwm_binding_t *binding = find_binding(timer, pin);
    if (binding == NULL) return select_driver(timer, pin) == NULL ? HAL_ERROR_NOT_SUPPORTED : HAL_ERROR_NOT_INITIALIZED;
    return binding->driver->stop(timer, pin);
}
