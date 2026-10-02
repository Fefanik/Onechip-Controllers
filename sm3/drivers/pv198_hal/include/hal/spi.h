#pragma once

#include "hal/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SPI_PORT_0 = 0,  /**< LPSPI0: MISO P0_14, MOSI P0_5, SCK P0_4, PCS P0_15. */
    SPI_PORT_1 = 1,  /**< LPSPI1: MISO P0_26, MOSI P0_24, SCK P0_25, PCS P0_27. */
    SPI_PORT_2 = 2,  /**< LPSPI2: MISO P4_2, MOSI P4_0, SCK P4_1, PCS P4_3. */
    SPI_PORT_3 = 3,  /**< LPSPI3: MISO P1_2, MOSI P1_0, SCK P1_1, PCS P1_3. */
    SPI_PORT_5 = 5,  /**< LPSPI5: MISO P1_18, MOSI P1_16, SCK P1_17, PCS P1_19. */
    SPI_PORT_6 = 6,  /**< LPSPI6: MISO P3_22, MOSI P3_20, SCK P3_21, PCS P3_23. */
    SPI_PORT_7 = 7,  /**< LPSPI7: MISO P3_4, MOSI P3_2, SCK P3_3, PCS P3_5. */
    SPI_PORT_9 = 9,  /**< LPSPI9: MISO P2_5, MOSI P2_4, SCK P2_3, PCS P2_2. */
    SPI_PORT_COUNT = 10,
} spi_port_t;

typedef enum {
    SPI_CLOCK_IDLE_LOW = 0,
    SPI_CLOCK_IDLE_HIGH,
} spi_clock_polarity_t;

typedef enum {
    SPI_CLOCK_PHASE_FIRST_EDGE = 0,
    SPI_CLOCK_PHASE_SECOND_EDGE,
} spi_clock_phase_t;

typedef enum {
    SPI_MSB_FIRST = 0,
    SPI_LSB_FIRST,
} spi_bit_order_t;

typedef struct {
    uint32_t frequency_hz;
    spi_clock_polarity_t clock_polarity;
    spi_clock_phase_t clock_phase;
    spi_bit_order_t bit_order;
} spi_master_config_t;

typedef struct {
    spi_clock_polarity_t clock_polarity;
    spi_clock_phase_t clock_phase;
    spi_bit_order_t bit_order;
} spi_slave_config_t;

/**
 * @brief Claim and initialize an SPI peripheral in master mode.
 * @param port SPI peripheral to initialize.
 * @param config Clock frequency and transfer format.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t spi_master_init(spi_port_t port,
                            const spi_master_config_t *config);

/**
 * @brief Disable and release an SPI master and its pins.
 * @param port SPI master to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t spi_master_deinit(spi_port_t port);

/**
 * @brief Perform a blocking full-duplex 8-bit SPI master transfer.
 * @param port Initialized SPI master.
 * @param tx Source buffer, or NULL to transmit 0xFF.
 * @param[out] rx Destination buffer, or NULL to discard received bytes.
 * @param count Number of bytes to transfer.
 * @return HAL_OK on success, otherwise a HAL error.
 *
 * Chip select is controlled separately through GPIO. There is no software
 * timeout. Interrupts must remain enabled; do not call from interrupt context.
 */
hal_error_t spi_master_transfer(spi_port_t port,
                                const uint8_t *tx,
                                uint8_t *rx,
                                size_t count);

/**
 * @brief SPI slave transaction interrupt handler.
 * @param port SPI slave that completed the transaction.
 * @param status Transaction status.
 * @param transferred Number of bytes exchanged.
 * @param context User context supplied during registration.
 */
typedef void (*spi_slave_isr_t)(spi_port_t port,
                                hal_error_t status,
                                size_t transferred,
                                void *context);

/**
 * @brief Claim and initialize an SPI peripheral in slave mode.
 * @param port SPI peripheral to initialize.
 * @param config Transfer format.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t spi_slave_init(spi_port_t port,
                           const spi_slave_config_t *config);

/**
 * @brief Disable and release an SPI slave and its pins.
 * @param port SPI slave to release.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t spi_slave_deinit(spi_port_t port);

/**
 * @brief Arm one interrupt-driven full-duplex SPI slave transaction.
 * @param port Initialized SPI slave.
 * @param tx Source buffer, or NULL to leave the output undriven.
 * @param[out] rx Destination buffer, or NULL to discard received bytes.
 * @param count Number of bytes to exchange.
 * @return HAL_OK when armed, otherwise a HAL error.
 *
 * At least one buffer must be non-NULL. Buffers must remain valid until the
 * registered interrupt handler runs. The handler may arm the next transaction.
 */
hal_error_t spi_slave_transfer(spi_port_t port,
                               const uint8_t *tx,
                               uint8_t *rx,
                               size_t count);

/**
 * @brief Register the transaction-complete interrupt for an SPI slave.
 * @param port Initialized SPI slave.
 * @param handler Handler invoked after each armed transaction.
 * @param context User context passed to the handler.
 * @return HAL_OK when registered, otherwise a HAL error.
 */
hal_error_t spi_slave_register_interrupt(spi_port_t port,
                                         spi_slave_isr_t handler,
                                         void *context);

/**
 * @brief Disable the receive interrupt for an SPI slave.
 * @param port Initialized SPI slave.
 * @return HAL_OK on success, otherwise a HAL error.
 */
hal_error_t spi_slave_unregister_interrupt(spi_port_t port);
