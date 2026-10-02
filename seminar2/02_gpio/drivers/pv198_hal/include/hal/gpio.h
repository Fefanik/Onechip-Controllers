#pragma once

#include "hal/error.h"
#include "hal/pinout.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GPIO_INPUT = 0,
    GPIO_OUTPUT,
} gpio_direction_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN,
} gpio_pull_t;

typedef struct {
    gpio_direction_t direction;
    gpio_pull_t pull;
    bool initial_level; /* Applied before output is enabled to avoid glitches. */
} gpio_config_t;

typedef enum {
    GPIO_RISING_EDGE = 0,
    GPIO_FALLING_EDGE,
    GPIO_ANY_EDGE,
} gpio_interrupt_t;

typedef enum {
    GPIO_PORT_0 = 0,
    GPIO_PORT_1,
    GPIO_PORT_2,
    GPIO_PORT_3,
    GPIO_PORT_4,
    GPIO_PORT_5,
    GPIO_PORT_COUNT,
} gpio_port_t;

/*
 * One handler may be registered per port. The callback executes in interrupt
 * context and receives the enabled pins that were pending. It must not block.
 * Interrupt flags represented by pending_mask are cleared before the callback.
 */
/**
 * @brief GPIO port interrupt callback.
 * @param port Port that generated the interrupt.
 * @param pending_mask Bits of enabled pins with pending interrupts.
 * @param context User context supplied during registration.
 */
typedef void (*gpio_port_isr_t)(gpio_port_t port,
                                uint32_t pending_mask,
                                void *context);

/**
 * @brief Claim an unused pin or reconfigure a GPIO-owned pin.
 * @param pin Pin to claim or reconfigure.
 * @param config New pin configuration.
 * @return HAL_OK on success, HAL_ERROR_BUSY for another owner, or a HAL error.
 */
hal_error_t gpio_init_pin(pin_num_t pin, const gpio_config_t *config);

/**
 * @brief Release a GPIO pin and return it to a safe input state.
 * @param pin Pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_deinit_pin(pin_num_t pin);

/**
 * @brief Read the logic level of a GPIO pin.
 * @param pin Pin to read.
 * @param[out] level Receives the sampled logic level.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_read_pin(pin_num_t pin, bool *level);

/**
 * @brief Set the output level of a GPIO pin.
 * @param pin Pin to write.
 * @param level Output logic level.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_write_pin(pin_num_t pin, bool level);

/**
 * @brief Toggle the output level of a GPIO pin.
 * @param pin Pin to toggle.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_toggle_pin(pin_num_t pin);

/**
 * @brief Claim or reconfigure selected GPIO pins; reject other peripheral owners.
 * @param port GPIO port.
 * @param pin_mask Bits selecting pins to claim or reconfigure.
 * @param config Configuration applied to every selected pin.
 * @return HAL_OK on success, HAL_ERROR_BUSY for another owner, or a HAL error.
 *
 * Validation or ownership failure leaves all selected pins unchanged.
 */
hal_error_t gpio_init_port(gpio_port_t port,
                          uint32_t pin_mask,
                          const gpio_config_t *config);

/**
 * @brief Release selected pins in a GPIO port.
 * @param port GPIO port.
 * @param pin_mask Bits selecting pins to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_deinit_port(gpio_port_t port, uint32_t pin_mask);

/**
 * @brief Read all 32 input levels of a GPIO port.
 * @param port GPIO port.
 * @param[out] value Receives the port value.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_read_port(gpio_port_t port, uint32_t *value);

/**
 * @brief Replace the output latch of an entire GPIO port.
 * @param port GPIO port.
 * @param value New 32-bit output value.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_write_port(gpio_port_t port, uint32_t value);

/**
 * @brief Replace selected bits in a GPIO port output latch.
 * @param port GPIO port.
 * @param pin_mask Bits to change.
 * @param value New values for the selected bits.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_write_port_masked(gpio_port_t port,
                                   uint32_t pin_mask,
                                   uint32_t value);

/**
 * @brief Atomically set selected GPIO port bits.
 * @param port GPIO port.
 * @param pin_mask Bits to set.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_set_port_bits(gpio_port_t port, uint32_t pin_mask);

/**
 * @brief Atomically clear selected GPIO port bits.
 * @param port GPIO port.
 * @param pin_mask Bits to clear.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_clear_port_bits(gpio_port_t port, uint32_t pin_mask);

/**
 * @brief Atomically toggle selected GPIO port bits.
 * @param port GPIO port.
 * @param pin_mask Bits to toggle.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_toggle_port_bits(gpio_port_t port, uint32_t pin_mask);

/**
 * @brief Register the interrupt handler for selected pins in a port.
 * @param port GPIO port.
 * @param pin_mask Bits selecting interrupt-enabled pins.
 * @param trigger Edge that generates an interrupt.
 * @param handler Callback invoked in interrupt context.
 * @param context User context passed to the callback.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_register_port_interrupt(gpio_port_t port,
                                         uint32_t pin_mask,
                                         gpio_interrupt_t trigger,
                                         gpio_port_isr_t handler,
                                         void *context);

/**
 * @brief Disable interrupts for selected pins in a port.
 * @param port GPIO port.
 * @param pin_mask Bits selecting pins to disable.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t gpio_unregister_port_interrupt(gpio_port_t port,
                                           uint32_t pin_mask);
