#pragma once

#include "hal/error.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    UART_PORT_0 = 0,  /**< LPUART0: TX P0_13, RX P0_12. */
    UART_PORT_1 = 1,  /**< LPUART1: TX P0_25, RX P0_24. */
    UART_PORT_2 = 2,  /**< LPUART2: TX P4_1, RX P4_0. */
    UART_PORT_3 = 3,  /**< LPUART3: TX P1_1, RX P1_0. */
    UART_PORT_5 = 5,  /**< LPUART5: TX P1_17, RX P1_16. */
    UART_PORT_6 = 6,  /**< LPUART6: TX P3_21, RX P3_20. */
    UART_PORT_7 = 7,  /**< LPUART7: TX P3_3, RX P3_2. */
    UART_PORT_9 = 9,  /**< LPUART9: TX P2_3, RX P2_4. */
    UART_PORT_COUNT = 10,
} uart_port_t;

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD,
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_2,
} uart_stop_bits_t;

typedef struct {
    uint32_t baud_rate;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
} uart_config_t;

/**
 * @brief Claim and initialize a UART with eight data bits.
 * @param port UART to initialize.
 * @param config Baud rate and serial format.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t uart_init(uart_port_t port, const uart_config_t *config);

/**
 * @brief Disable and release a UART and its fixed pins.
 * @param port UART to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t uart_deinit(uart_port_t port);

/* No software timeout: reads can wait indefinitely for incoming data.
 * Use uart_available() before reading if the caller must avoid waiting. */
/**
 * @brief Write bytes, blocking until the last byte finishes transmission.
 * @param port Initialized UART.
 * @param data Source buffer.
 * @param count Number of bytes to write.
 * @return HAL_OK after the entire buffer finishes transmission, otherwise a HAL error.
 * @note On error, some bytes may already have been sent; partial progress is not reported.
 */
hal_error_t uart_write(uart_port_t port,
                       const uint8_t *data,
                       size_t count);

/**
 * @brief Query how many received bytes can be read without waiting.
 * @param port Initialized UART.
 * @param[out] available Receives the number of buffered bytes.
 * @return HAL_OK on success, otherwise a HAL error.
 *
 * This function does not consume any received data.
 */
hal_error_t uart_available(uart_port_t port, size_t *available);

/**
 * @brief Read bytes, blocking until the buffer is full or a receive error occurs.
 * @param port Initialized UART.
 * @param[out] data Destination buffer.
 * @param count Number of bytes requested.
 * @param[out] received Receives the number of bytes read, including on error.
 * @return HAL_OK when all bytes are read, otherwise a HAL error.
 */
hal_error_t uart_read(uart_port_t port,
                      uint8_t *data,
                      size_t count,
                      size_t *received);
