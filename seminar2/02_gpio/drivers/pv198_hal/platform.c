#include "internal.h"

#include "fsl_clock.h"

#include <string.h>

static uint8_t pin_owners[PIN_ENCODED_LIMIT];
static uint8_t flexcomm_owners[10];
static uint8_t ctimer_owners[5];

bool hal_pin_valid(pin_num_t pin)
{
    return (unsigned)pin < PIN_ENCODED_LIMIT;
}

GPIO_Type *hal_gpio_base(unsigned port)
{
    static GPIO_Type *const bases[] = {GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5};
    return port < GPIO_PORT_COUNT ? bases[port] : NULL;
}

PORT_Type *hal_port_base(unsigned port)
{
    static PORT_Type *const bases[] = {PORT0, PORT1, PORT2, PORT3, PORT4, PORT5};
    return port < GPIO_PORT_COUNT ? bases[port] : NULL;
}

static void enable_pin_clocks(unsigned port)
{
    static const clock_ip_name_t gpio_clocks[] = {
        kCLOCK_Gpio0, kCLOCK_Gpio1, kCLOCK_Gpio2, kCLOCK_Gpio3, kCLOCK_Gpio4,
    };
    static const clock_ip_name_t port_clocks[] = {
        kCLOCK_Port0, kCLOCK_Port1, kCLOCK_Port2, kCLOCK_Port3, kCLOCK_Port4,
    };
    /* Port 5 belongs to the always-on domain and has no SYSCON gate. */
    if (port < sizeof(gpio_clocks) / sizeof(gpio_clocks[0])) {
        CLOCK_EnableClock(gpio_clocks[port]);
        CLOCK_EnableClock(port_clocks[port]);
    }
}

hal_error_t hal_pin_claim(pin_num_t pin, hal_pin_owner_t owner)
{
    if (!hal_pin_valid(pin) || owner == HAL_PIN_OWNER_NONE) {
        return HAL_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t primask = DisableGlobalIRQ();
    const uint8_t previous = pin_owners[(unsigned)pin];
    if (previous == HAL_PIN_OWNER_NONE) {
        pin_owners[(unsigned)pin] = (uint8_t)owner;
    }
    EnableGlobalIRQ(primask);
    return previous == HAL_PIN_OWNER_NONE ? HAL_OK : HAL_ERROR_ALREADY_INITIALIZED;
}

void hal_pin_release(pin_num_t pin, hal_pin_owner_t owner)
{
    if (!hal_pin_valid(pin)) {
        return;
    }
    const uint32_t primask = DisableGlobalIRQ();
    if (pin_owners[(unsigned)pin] == (uint8_t)owner) {
        pin_owners[(unsigned)pin] = HAL_PIN_OWNER_NONE;
    }
    EnableGlobalIRQ(primask);
}

bool hal_pin_owned_by(pin_num_t pin, hal_pin_owner_t owner)
{
    return hal_pin_valid(pin) && pin_owners[(unsigned)pin] == (uint8_t)owner;
}

hal_error_t hal_flexcomm_claim(unsigned instance, hal_pin_owner_t owner)
{
    if (instance >= sizeof(flexcomm_owners) || owner == HAL_PIN_OWNER_NONE)
        return HAL_ERROR_INVALID_ARGUMENT;
    const uint32_t primask = DisableGlobalIRQ();
    const uint8_t previous = flexcomm_owners[instance];
    if (previous == HAL_PIN_OWNER_NONE) flexcomm_owners[instance] = (uint8_t)owner;
    EnableGlobalIRQ(primask);
    return previous == HAL_PIN_OWNER_NONE ? HAL_OK : HAL_ERROR_BUSY;
}

void hal_flexcomm_release(unsigned instance, hal_pin_owner_t owner)
{
    if (instance >= sizeof(flexcomm_owners)) return;
    const uint32_t primask = DisableGlobalIRQ();
    if (flexcomm_owners[instance] == (uint8_t)owner)
        flexcomm_owners[instance] = HAL_PIN_OWNER_NONE;
    EnableGlobalIRQ(primask);
}

uint32_t hal_flexcomm_enable_clock(unsigned instance)
{
    switch (instance) {
    case 0u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0); CLOCK_SetClkDiv(kCLOCK_DivFlexcom0Clk, 1u); break;
    case 1u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM1); CLOCK_SetClkDiv(kCLOCK_DivFlexcom1Clk, 1u); break;
    case 2u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2); CLOCK_SetClkDiv(kCLOCK_DivFlexcom2Clk, 1u); break;
    case 3u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3); CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u); break;
    case 4u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM4); CLOCK_SetClkDiv(kCLOCK_DivFlexcom4Clk, 1u); break;
    case 5u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM5); CLOCK_SetClkDiv(kCLOCK_DivFlexcom5Clk, 1u); break;
    case 6u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM6); CLOCK_SetClkDiv(kCLOCK_DivFlexcom6Clk, 1u); break;
    case 7u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM7); CLOCK_SetClkDiv(kCLOCK_DivFlexcom7Clk, 1u); break;
    case 8u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM8); CLOCK_SetClkDiv(kCLOCK_DivFlexcom8Clk, 1u); break;
    case 9u: CLOCK_AttachClk(kFRO12M_to_FLEXCOMM9); CLOCK_SetClkDiv(kCLOCK_DivFlexcom9Clk, 1u); break;
    default: return 0u;
    }
    return CLOCK_GetLPFlexCommClkFreq(instance);
}

hal_error_t hal_ctimer_claim(unsigned instance, hal_ctimer_owner_t owner)
{
    if (instance >= sizeof(ctimer_owners) || owner == HAL_CTIMER_OWNER_NONE)
        return HAL_ERROR_INVALID_ARGUMENT;
    const uint32_t primask = DisableGlobalIRQ();
    const uint8_t previous = ctimer_owners[instance];
    if (previous == HAL_CTIMER_OWNER_NONE)
        ctimer_owners[instance] = (uint8_t)owner;
    EnableGlobalIRQ(primask);
    return previous == HAL_CTIMER_OWNER_NONE ? HAL_OK : HAL_ERROR_BUSY;
}

void hal_ctimer_release(unsigned instance, hal_ctimer_owner_t owner)
{
    if (instance >= sizeof(ctimer_owners)) return;
    const uint32_t primask = DisableGlobalIRQ();
    if (ctimer_owners[instance] == (uint8_t)owner)
        ctimer_owners[instance] = HAL_CTIMER_OWNER_NONE;
    EnableGlobalIRQ(primask);
}

bool hal_ctimer_owned_by(unsigned instance, hal_ctimer_owner_t owner)
{
    return instance < sizeof(ctimer_owners)
        && ctimer_owners[instance] == (uint8_t)owner;
}

void hal_pin_configure(pin_num_t pin,
                       port_mux_t mux,
                       gpio_pull_t pull,
                       bool open_drain,
                       bool input_enable)
{
    const unsigned port = PIN_PORT(pin);
    enable_pin_clocks(port);
    const port_pin_config_t config = {
        .pullSelect = pull == GPIO_PULL_UP ? kPORT_PullUp
                    : pull == GPIO_PULL_DOWN ? kPORT_PullDown
                                             : kPORT_PullDisable,
        .pullValueSelect = kPORT_LowPullResistor,
        .slewRate = kPORT_FastSlewRate,
        .passiveFilterEnable = kPORT_PassiveFilterDisable,
        .openDrainEnable = open_drain ? kPORT_OpenDrainEnable : kPORT_OpenDrainDisable,
        .driveStrength = kPORT_LowDriveStrength,
        .mux = mux,
        .inputBuffer = input_enable ? kPORT_InputBufferEnable : kPORT_InputBufferDisable,
        .invertInput = kPORT_InputNormal,
        .lockRegister = kPORT_UnlockRegister,
    };
    PORT_SetPinConfig(hal_port_base(port), PIN_INDEX(pin), &config);
}

void hal_pin_make_safe(pin_num_t pin)
{
    hal_pin_configure(pin, kPORT_MuxAlt0, GPIO_PULL_NONE, false, true);
    const gpio_pin_config_t input = {.pinDirection = kGPIO_DigitalInput, .outputLogic = 0u};
    GPIO_PinInit(hal_gpio_base(PIN_PORT(pin)), PIN_INDEX(pin), &input);
}
