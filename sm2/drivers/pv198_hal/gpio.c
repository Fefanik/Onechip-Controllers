#include "hal/gpio.h"

#include "internal.h"

#include "fsl_common.h"
#include "fsl_gpio.h"

typedef struct {
    uint32_t enabled_mask;
    gpio_port_isr_t handler;
    void *context;
} gpio_irq_state_t;

static gpio_irq_state_t irq_states[GPIO_PORT_COUNT];

static bool port_valid(gpio_port_t port)
{
    return (unsigned)port < GPIO_PORT_COUNT;
}

static bool config_valid(const gpio_config_t *config)
{
    return config != NULL
        && (config->direction == GPIO_INPUT || config->direction == GPIO_OUTPUT)
        && (config->pull == GPIO_PULL_NONE || config->pull == GPIO_PULL_UP
            || config->pull == GPIO_PULL_DOWN);
}

static hal_error_t configure_owned_pin(pin_num_t pin, const gpio_config_t *config)
{
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) {
        return HAL_ERROR_NOT_INITIALIZED;
    }
    if (!config_valid(config)) {
        return HAL_ERROR_INVALID_ARGUMENT;
    }
    hal_pin_configure(pin, kPORT_MuxAlt0, config->pull, false, true);
    GPIO_Type *const base = hal_gpio_base(PIN_PORT(pin));
    const unsigned index = PIN_INDEX(pin);
    if (config->direction == GPIO_OUTPUT) {
        GPIO_PinWrite(base, index, config->initial_level ? 1u : 0u);
    }
    const gpio_pin_config_t sdk_config = {
        .pinDirection = config->direction == GPIO_OUTPUT ? kGPIO_DigitalOutput : kGPIO_DigitalInput,
        .outputLogic = config->initial_level ? 1u : 0u,
    };
    GPIO_PinInit(base, index, &sdk_config);
    return HAL_OK;
}

hal_error_t gpio_init_pin(pin_num_t pin, const gpio_config_t *config)
{
    if (!hal_pin_valid(pin) || !config_valid(config))
        return HAL_ERROR_INVALID_ARGUMENT;
    const uint32_t saved = DisableGlobalIRQ();
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) {
        if (hal_pin_claim(pin, HAL_PIN_OWNER_GPIO) != HAL_OK) {
            EnableGlobalIRQ(saved);
            return HAL_ERROR_BUSY;
        }
    }
    const hal_error_t status = configure_owned_pin(pin, config);
    EnableGlobalIRQ(saved);
    return status;
}

hal_error_t gpio_deinit_pin(pin_num_t pin)
{
    if (!hal_pin_valid(pin)) return HAL_ERROR_INVALID_ARGUMENT;
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) return HAL_ERROR_NOT_INITIALIZED;
    hal_pin_make_safe(pin);
    hal_pin_release(pin, HAL_PIN_OWNER_GPIO);
    return HAL_OK;
}

hal_error_t gpio_read_pin(pin_num_t pin, bool *level)
{
    if (level == NULL || !hal_pin_valid(pin)) return HAL_ERROR_INVALID_ARGUMENT;
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) return HAL_ERROR_NOT_INITIALIZED;
    *level = GPIO_PinRead(hal_gpio_base(PIN_PORT(pin)), PIN_INDEX(pin)) != 0u;
    return HAL_OK;
}

hal_error_t gpio_write_pin(pin_num_t pin, bool level)
{
    if (!hal_pin_valid(pin)) return HAL_ERROR_INVALID_ARGUMENT;
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) return HAL_ERROR_NOT_INITIALIZED;
    GPIO_PinWrite(hal_gpio_base(PIN_PORT(pin)), PIN_INDEX(pin), level ? 1u : 0u);
    return HAL_OK;
}

hal_error_t gpio_toggle_pin(pin_num_t pin)
{
    if (!hal_pin_valid(pin)) return HAL_ERROR_INVALID_ARGUMENT;
    if (!hal_pin_owned_by(pin, HAL_PIN_OWNER_GPIO)) return HAL_ERROR_NOT_INITIALIZED;
    GPIO_PortToggle(hal_gpio_base(PIN_PORT(pin)), 1u << PIN_INDEX(pin));
    return HAL_OK;
}

hal_error_t gpio_init_port(gpio_port_t port, uint32_t mask, const gpio_config_t *config)
{
    if (!port_valid(port) || mask == 0u || !config_valid(config))
        return HAL_ERROR_INVALID_ARGUMENT;
    /* Validate and reserve the whole mask before changing any pin configuration. */
    for (unsigned pin = 0; pin < 32u; ++pin) {
        if ((mask & (1u << pin)) != 0u
            && !hal_pin_valid((pin_num_t)PIN_ENCODE(port, pin)))
            return HAL_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t saved = DisableGlobalIRQ();
    uint32_t claimed = 0u;
    for (unsigned pin = 0; pin < 32u; ++pin) {
        const uint32_t bit = 1u << pin;
        const pin_num_t number = (pin_num_t)PIN_ENCODE(port, pin);
        if ((mask & bit) == 0u || hal_pin_owned_by(number, HAL_PIN_OWNER_GPIO))
            continue;
        if (hal_pin_claim(number, HAL_PIN_OWNER_GPIO) != HAL_OK) {
            for (unsigned rollback = 0; rollback < 32u; ++rollback) {
                if ((claimed & (1u << rollback)) != 0u)
                    hal_pin_release((pin_num_t)PIN_ENCODE(port, rollback),
                                    HAL_PIN_OWNER_GPIO);
            }
            EnableGlobalIRQ(saved);
            return HAL_ERROR_BUSY;
        }
        claimed |= bit;
    }
    for (unsigned pin = 0; pin < 32u; ++pin) {
        if ((mask & (1u << pin)) != 0u)
            (void)configure_owned_pin((pin_num_t)PIN_ENCODE(port, pin), config);
    }
    EnableGlobalIRQ(saved);
    return HAL_OK;
}

hal_error_t gpio_deinit_port(gpio_port_t port, uint32_t mask)
{
    if (!port_valid(port) || mask == 0u) return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t result = HAL_OK;
    for (unsigned pin = 0; pin < 32u; ++pin) {
        if ((mask & (1u << pin)) != 0u
            && gpio_deinit_pin((pin_num_t)PIN_ENCODE(port, pin)) != HAL_OK) {
            result = HAL_ERROR_NOT_INITIALIZED;
        }
    }
    return result;
}

hal_error_t gpio_read_port(gpio_port_t port, uint32_t *value)
{
    if (!port_valid(port) || value == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    *value = hal_gpio_base(port)->PDIR;
    return HAL_OK;
}

hal_error_t gpio_write_port(gpio_port_t port, uint32_t value)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    hal_gpio_base(port)->PDOR = value;
    return HAL_OK;
}

hal_error_t gpio_write_port_masked(gpio_port_t port, uint32_t mask, uint32_t value)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    GPIO_Type *const base = hal_gpio_base(port);
    const uint32_t irq = DisableGlobalIRQ();
    base->PDOR = (base->PDOR & ~mask) | (value & mask);
    EnableGlobalIRQ(irq);
    return HAL_OK;
}

hal_error_t gpio_set_port_bits(gpio_port_t port, uint32_t mask)
{ if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT; GPIO_PortSet(hal_gpio_base(port), mask); return HAL_OK; }
hal_error_t gpio_clear_port_bits(gpio_port_t port, uint32_t mask)
{ if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT; GPIO_PortClear(hal_gpio_base(port), mask); return HAL_OK; }
hal_error_t gpio_toggle_port_bits(gpio_port_t port, uint32_t mask)
{ if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT; GPIO_PortToggle(hal_gpio_base(port), mask); return HAL_OK; }

hal_error_t gpio_register_port_interrupt(gpio_port_t port, uint32_t mask,
                                         gpio_interrupt_t trigger,
                                         gpio_port_isr_t handler, void *context)
{
    if (!port_valid(port) || mask == 0u || handler == NULL
        || (unsigned)trigger > GPIO_ANY_EDGE) return HAL_ERROR_INVALID_ARGUMENT;
    gpio_irq_state_t *const state = &irq_states[port];
    if (state->handler != NULL && state->handler != handler) return HAL_ERROR_BUSY;
    gpio_interrupt_config_t sdk_trigger = trigger == GPIO_RISING_EDGE ? kGPIO_InterruptRisingEdge
        : trigger == GPIO_FALLING_EDGE ? kGPIO_InterruptFallingEdge : kGPIO_InterruptEitherEdge;
    GPIO_SetMultipleInterruptPinsConfig(hal_gpio_base(port), mask, sdk_trigger);
    state->enabled_mask |= mask;
    state->handler = handler;
    state->context = context;
    static const IRQn_Type irqs[] = {GPIO00_IRQn, GPIO10_IRQn, GPIO20_IRQn, GPIO30_IRQn, GPIO40_IRQn, GPIO50_IRQn};
    EnableIRQ(irqs[port]);
    return HAL_OK;
}

hal_error_t gpio_unregister_port_interrupt(gpio_port_t port, uint32_t mask)
{
    if (!port_valid(port) || mask == 0u) return HAL_ERROR_INVALID_ARGUMENT;
    gpio_irq_state_t *const state = &irq_states[port];
    if ((state->enabled_mask & mask) == 0u) return HAL_ERROR_NOT_INITIALIZED;
    GPIO_SetMultipleInterruptPinsConfig(hal_gpio_base(port), mask, kGPIO_InterruptStatusFlagDisabled);
    state->enabled_mask &= ~mask;
    if (state->enabled_mask == 0u) { state->handler = NULL; state->context = NULL; }
    return HAL_OK;
}

static void gpio_irq(unsigned port)
{
    gpio_irq_state_t *const state = &irq_states[port];
    const uint32_t pending = GPIO_GpioGetInterruptChannelFlags(hal_gpio_base(port), 0u)
                           & state->enabled_mask;
    GPIO_GpioClearInterruptChannelFlags(hal_gpio_base(port), pending, 0u);
    if (pending != 0u && state->handler != NULL) state->handler((gpio_port_t)port, pending, state->context);
}

void GPIO00_DriverIRQHandler(void) { gpio_irq(0u); }
void GPIO10_DriverIRQHandler(void) { gpio_irq(1u); }
void GPIO20_DriverIRQHandler(void) { gpio_irq(2u); }
void GPIO30_DriverIRQHandler(void) { gpio_irq(3u); }
void GPIO40_DriverIRQHandler(void) { gpio_irq(4u); }
void GPIO50_DriverIRQHandler(void) { gpio_irq(5u); }
