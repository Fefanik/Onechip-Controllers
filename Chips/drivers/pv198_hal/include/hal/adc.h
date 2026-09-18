#pragma once

#include "hal/error.h"
#include "hal/pinout.h"

#include <stdint.h>

/*
 * Raw results are normalized to 0..UINT16_MAX, where zero is ground and
 * UINT16_MAX is the board's nominal analog reference voltage.
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
 * @param pin ADC-capable input pin.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_init(pin_num_t pin);

/**
 * @brief Release an ADC channel and its pin.
 * @param pin ADC pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_deinit(pin_num_t pin);

/**
 * @brief Perform a blocking raw ADC conversion.
 * @param pin Initialized ADC pin.
 * @param[out] sample Receives a normalized 16-bit sample.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_read_raw(pin_num_t pin, uint16_t *sample);

/**
 * @brief Perform a blocking ADC conversion in millivolts.
 * @param pin Initialized ADC pin.
 * @param[out] millivolts Receives the nominal input voltage.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_read_mv(pin_num_t pin, uint32_t *millivolts);

/**
 * @brief Register a conversion-complete interrupt handler for an ADC pin.
 * @param pin Initialized ADC pin.
 * @param handler Callback invoked after each started conversion.
 * @param context User context passed to the callback.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_register_interrupt(pin_num_t pin,
                                   adc_isr_t handler,
                                   void *context);

/**
 * @brief Start one interrupt-driven ADC conversion.
 * @param pin ADC pin with a registered interrupt handler.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t adc_start_conversion(pin_num_t pin);

/**
 * @brief Remove an ADC conversion-complete interrupt handler.
 * @param pin ADC pin whose handler is removed.
 * @return HAL_OK on success, otherwise a HAL error.
 * @note A conversion already started still invokes its handler.
 */
hal_error_t adc_unregister_interrupt(pin_num_t pin);
