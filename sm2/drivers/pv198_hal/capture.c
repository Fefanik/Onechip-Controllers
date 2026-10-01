#include "hal/capture.h"

#include "hal/drivers/capture.h"

#include <stddef.h>

static const capture_driver_t *select_driver(timer_num_t timer, pin_num_t pin)
{
    const capture_driver_t *const drivers[] = {get_ctimer_capture_driver()};
    for (unsigned i = 0; i < sizeof(drivers) / sizeof(drivers[0]); ++i)
        if (drivers[i]->supports(timer, pin)) return drivers[i];
    return NULL;
}

hal_error_t capture_init(timer_num_t timer, pin_num_t pin, capture_edge_t edge,
                         capture_isr_t handler, void *context)
{
    if ((unsigned)timer > TIMER4 || (unsigned)pin >= PIN_ENCODED_LIMIT || handler == NULL
        || (unsigned)edge > CAPTURE_ANY)
        return HAL_ERROR_INVALID_ARGUMENT;
    const capture_driver_t *const driver = select_driver(timer, pin);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    return driver->init(timer, pin, edge, handler, context);
}

hal_error_t capture_deinit(timer_num_t timer, pin_num_t pin)
{
    if ((unsigned)timer > TIMER4 || (unsigned)pin >= PIN_ENCODED_LIMIT) return HAL_ERROR_INVALID_ARGUMENT;
    const capture_driver_t *const driver = select_driver(timer, pin);
    if (driver == NULL) return HAL_ERROR_NOT_SUPPORTED;
    return driver->deinit(timer, pin);
}
