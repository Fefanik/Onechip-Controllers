#pragma once

#include "hal/error.h"
#include "hal/pinout.h"

#include <stdint.h>

/*
 * Raw values are normalized to 0..UINT16_MAX, where zero is ground and
 * UINT16_MAX is the board's nominal analog reference voltage.
 */

/**
 * @brief Claim a DAC-capable pin and enable its analog output.
 * @param pin DAC-capable output pin.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t dac_init(pin_num_t pin);

/**
 * @brief Disable a DAC output and release its pin.
 * @param pin DAC pin to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t dac_deinit(pin_num_t pin);

/**
 * @brief Set a normalized raw DAC output value.
 * @param pin Initialized DAC pin.
 * @param value Normalized 16-bit output value.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t dac_write_raw(pin_num_t pin, uint16_t value);

/**
 * @brief Set a DAC output voltage in millivolts.
 * @param pin Initialized DAC pin.
 * @param millivolts Output voltage from 0 to 3300 mV.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t dac_write_mv(pin_num_t pin, uint32_t millivolts);
