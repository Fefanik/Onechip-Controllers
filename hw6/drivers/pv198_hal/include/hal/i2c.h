#pragma once

#include "hal/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    I2C_PORT_0 = 0,  /**< LPI2C0: SDA P0_12, SCL P0_13. */
    I2C_PORT_1 = 1,  /**< LPI2C1: SDA P0_24, SCL P0_25. */
    I2C_PORT_2 = 2,  /**< LPI2C2: SDA P4_0, SCL P4_1. */
    I2C_PORT_3 = 3,  /**< LPI2C3: SDA P1_0, SCL P1_1. */
    I2C_PORT_5 = 5,  /**< LPI2C5: SDA P1_16, SCL P1_17. */
    I2C_PORT_6 = 6,  /**< LPI2C6: SDA P3_20, SCL P3_21. */
    I2C_PORT_7 = 7,  /**< LPI2C7: SDA P3_2, SCL P3_3. */
    I2C_PORT_9 = 9,  /**< LPI2C9: SDA P2_4, SCL P2_3. */
    I2C_PORT_COUNT = 10,
} i2c_port_t;

typedef struct {
    uint32_t frequency_hz;
} i2c_config_t;

/**
 * @brief Claim and initialize an I2C controller.
 * @param port Controller to initialize.
 * @param config Bus frequency.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t i2c_init(i2c_port_t port, const i2c_config_t *config);

/**
 * @brief Disable and release an I2C controller and its pins.
 * @param port Controller to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t i2c_deinit(i2c_port_t port);

/*
 * address is an unshifted 7-bit address in the range 0x08..0x77.
 * Transfers block until completion or a hardware error, with no software deadline.
 * A nonzero count requires a non-NULL buffer. Interrupts must remain enabled;
 * do not call these transfers from interrupt context.
 */
/**
 * @brief Read bytes in one I2C transaction.
 * @param port Initialized controller.
 * @param address Unshifted 7-bit target address.
 * @param[out] buffer Destination buffer.
 * @param count Number of bytes to read.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t i2c_read(i2c_port_t port,
                     uint8_t address,
                     uint8_t *buffer,
                     size_t count);

/**
 * @brief Write bytes in one I2C transaction.
 * @param port Initialized controller.
 * @param address Unshifted 7-bit target address.
 * @param buffer Source buffer.
 * @param count Number of bytes to write.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t i2c_write(i2c_port_t port,
                      uint8_t address,
                      const uint8_t *buffer,
                      size_t count);

/**
 * @brief Write and then read using a repeated START.
 * @param port Initialized controller.
 * @param address Unshifted 7-bit target address.
 * @param write_buffer Source buffer.
 * @param write_count Number of bytes to write.
 * @param[out] read_buffer Destination buffer.
 * @param read_count Number of bytes to read.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t i2c_write_read(i2c_port_t port,
                           uint8_t address,
                           const uint8_t *write_buffer,
                           size_t write_count,
                           uint8_t *read_buffer,
                           size_t read_count);
