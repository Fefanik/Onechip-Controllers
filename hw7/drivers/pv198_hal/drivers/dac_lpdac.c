#include "hal/drivers/dac.h"

#include "internal.h"

#include "fsl_clock.h"
#include "fsl_dac.h"
#include "fsl_spc.h"

#include <stdbool.h>

typedef struct {
    pin_num_t pin;
    LPDAC_Type *base;
    unsigned instance;
} dac_route_t;

static bool initialized[2];

static const dac_route_t routes[] = {
    {PIN_4_2, DAC0, 0u},
    {PIN_4_3, DAC1, 1u},
};

static const dac_route_t *find_route(pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        if (routes[i].pin == pin) return &routes[i];
    return NULL;
}

static bool supports(pin_num_t pin)
{
    return find_route(pin) != NULL;
}

static void enable_clock(unsigned instance)
{
    if (instance == 0u) {
        CLOCK_SetClkDiv(kCLOCK_DivDac0Clk, 1u);
        CLOCK_AttachClk(kFRO_HF_to_DAC0);
    } else {
        CLOCK_SetClkDiv(kCLOCK_DivDac1Clk, 1u);
        CLOCK_AttachClk(kFRO_HF_to_DAC1);
    }
}

static hal_error_t init(pin_num_t pin)
{
    const dac_route_t *route = find_route(pin);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (initialized[route->instance]) return HAL_ERROR_ALREADY_INITIALIZED;

    hal_error_t status = hal_pin_claim(pin, HAL_PIN_OWNER_DAC);
    if (status != HAL_OK) return status;

    hal_pin_configure(pin, kPORT_MuxAlt0, GPIO_PULL_NONE, false, false);
    enable_clock(route->instance);
    const uint32_t analog_mask = route->instance == 0u
                               ? kSPC_controlDac0 : kSPC_controlDac1;
    SPC_EnableActiveModeAnalogModules(SPC0,
                                      kSPC_controlVref | analog_mask);

    dac_config_t config;
    DAC_GetDefaultConfig(&config);
    config.referenceVoltageSource = kDAC_ReferenceVoltageSourceAlt1;
    DAC_Init(route->base, &config);
    DAC_SetData(route->base, 0u);
    DAC_Enable(route->base, true);
    initialized[route->instance] = true;
    return HAL_OK;
}

static hal_error_t deinit(pin_num_t pin)
{
    const dac_route_t *route = find_route(pin);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (!initialized[route->instance]) return HAL_ERROR_NOT_INITIALIZED;

    DAC_SetData(route->base, 0u);
    DAC_Deinit(route->base);
    const uint32_t analog_mask = route->instance == 0u
                               ? kSPC_controlDac0 : kSPC_controlDac1;
    SPC_DisableActiveModeAnalogModules(SPC0, analog_mask);
    initialized[route->instance] = false;
    hal_pin_make_safe(pin);
    hal_pin_release(pin, HAL_PIN_OWNER_DAC);
    return HAL_OK;
}

static hal_error_t write_raw(pin_num_t pin, uint16_t value)
{
    const dac_route_t *route = find_route(pin);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (!initialized[route->instance]) return HAL_ERROR_NOT_INITIALIZED;

    const uint32_t hardware_value = ((uint32_t)value * 4095u + 32767u)
                                  / 65535u;
    DAC_SetData(route->base, hardware_value);
    return HAL_OK;
}

static const dac_driver_t driver = {
    .supports = supports,
    .init = init,
    .deinit = deinit,
    .write_raw = write_raw,
};

const dac_driver_t *get_lpdac_driver(void) { return &driver; }
