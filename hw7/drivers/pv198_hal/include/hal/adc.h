#pragma once

#include "hal/error.h"
#include "hal/pinout.h"

#include <stdint.h>

/*
 * Raw results are normalized to 0..UINT16_MAX, where zero is ground and
 * UINT16_MAX is the board's nominal analog reference voltage.
 * Dedicated analog inputs PIN_ANA_0, PIN_ANA_1, PIN_ANA_4, PIN_ANA_5,
 * PIN_ANA_6, and PIN_ANA_7 use the same API as digital ADC-capable pins.
 * PIN_ANA_6 and PIN_ANA_7 use ADC0. The OPAMP-only PIN_ANA_14/18/22 pads
 * are not direct ADC inputs. ANA_7 shares VREFI/VREFO: do not drive it as
 * an ordinary input or alter the board's reference wiring to take a reading.
 */

/**
 * @brief ADC conversion-complete callback; executes in interrupt context.
 * @param pin Pin that was measured.
 * @param sample Normalized 16-bit sample.
 * @param context User context supplied during registration.
 */
typedef void (*adc_isr_t)(pin_num_t pin, uint16_t sample, void *context);

/**
 * @brief Claim an ADC-capable pin and initialize its converter channel.
 * @param pin ADC-capable digital pin or dedicated PIN_ANA_* input listed above.
 * @return HAL_OK on success, HAL_ERROR_NOT_SUPPORTED for a pin without an ADC
 *         route, HAL_ERROR_ALREADY_INITIALIZED if already initialized, or
 *         HAL_ERROR_BUSY if all eight input bindings are occupied. A pin owned
 *         by another peripheral returns the ownership error.
 * @note Dedicated analog pads have no GPIO mux. Initialization claims the pad
 *       and initializes its converter without changing PORT/GPIO registers.
 */
hal_error_t adc_init(pin_num_t pin);

/**
 * @brief Release an ADC channel and its pin.
 * @param pin ADC pin to release.
 * @return HAL_OK on success, HAL_ERROR_BUSY if this pin has a queued/active
 *         conversion or its converter is in use, HAL_ERROR_NOT_INITIALIZED
 *         if not initialized, or HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 */
hal_error_t adc_deinit(pin_num_t pin);

/**
 * @brief Perform a blocking raw ADC conversion.
 * @param pin Initialized ADC pin.
 * @param[out] sample Receives a normalized 16-bit sample.
 * @return HAL_OK on success, HAL_ERROR_BUSY while any ADC conversion or
 *         completion callback is in progress, HAL_ERROR_INVALID_ARGUMENT for
 *         a NULL output, HAL_ERROR_NOT_INITIALIZED if not initialized, or
 *         HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 * @note Blocking reads are not queued, and share the reservation across both
 *       converters with interrupt-driven requests.
 */
hal_error_t adc_read_raw(pin_num_t pin, uint16_t *sample);

/**
 * @brief Perform a blocking ADC conversion in millivolts.
 * @param pin Initialized ADC pin.
 * @param[out] millivolts Receives the nominal input voltage.
 * @return HAL_OK on success, HAL_ERROR_BUSY while any ADC conversion or
 *         completion callback is in progress, HAL_ERROR_INVALID_ARGUMENT for
 *         a NULL output, HAL_ERROR_NOT_INITIALIZED if not initialized, or
 *         HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 */
hal_error_t adc_read_mv(pin_num_t pin, uint32_t *millivolts);

/**
 * @brief Register a conversion-complete interrupt handler for an ADC pin.
 * @param pin Initialized ADC pin.
 * @param handler Callback invoked after each started conversion.
 * @param context User context passed to the callback.
 * @return HAL_OK on success, HAL_ERROR_INVALID_ARGUMENT for a NULL handler,
 *         HAL_ERROR_ALREADY_INITIALIZED if a handler is already registered,
 *         HAL_ERROR_NOT_INITIALIZED if the pin is not initialized, or
 *         HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 */
hal_error_t adc_register_interrupt(pin_num_t pin,
                                   adc_isr_t handler,
                                   void *context);

/**
 * @brief Queue one interrupt-driven ADC conversion.
 * @param pin ADC pin with a registered interrupt handler.
 * @return HAL_OK when accepted (not necessarily started), HAL_ERROR_BUSY if
 *         this pin already has a queued/active request or the queue is full,
 *         HAL_ERROR_NOT_INITIALIZED if the pin or handler is not initialized,
 *         or HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 * @note Requests run in FIFO order, one hardware conversion at a time across
 *       ADC0 and ADC1. There is space for all eight initialized inputs, with
 *       at most one outstanding request per pin. The handler and context are
 *       captured when accepted; keep that context valid until completion.
 * @note The completed pin becomes available before its callback runs. Calling
 *       this function from that callback appends a new request at the tail.
 *       The next conversion starts only after the callback returns. Callbacks
 *       must remain short; serialization does not guarantee foreground CPU time.
 */
hal_error_t adc_start_conversion(pin_num_t pin);

/**
 * @brief Remove an ADC conversion-complete interrupt handler.
 * @param pin ADC pin whose handler is removed.
 * @return HAL_OK on success, HAL_ERROR_NOT_INITIALIZED if the pin or handler
 *         is not initialized, or HAL_ERROR_NOT_SUPPORTED for an unsupported pin.
 * @note Queued and active conversions still invoke their captured handlers;
 *       unregistering does not cancel requests or shorten context lifetime.
 */
hal_error_t adc_unregister_interrupt(pin_num_t pin);
