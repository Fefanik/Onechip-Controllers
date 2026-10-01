#include "hal/uart.h"

#include "internal.h"

#include "fsl_clock.h"
#include "fsl_lpuart.h"

#include <stdbool.h>

typedef struct {
    pin_num_t rx;
    pin_num_t tx;
    LPUART_Type *base;
    unsigned instance;
    port_mux_t mux;
} uart_route_t;

typedef struct {
    bool initialized;
    const uart_route_t *route;
} uart_state_t;

static const uart_route_t routes[] = {
    {PIN_0_12, PIN_0_13, LPUART0, 0u, kPORT_MuxAlt3},
    {PIN_0_24, PIN_0_25, LPUART1, 1u, kPORT_MuxAlt2},
    {PIN_4_0,  PIN_4_1,  LPUART2, 2u, kPORT_MuxAlt2},
    {PIN_1_0,  PIN_1_1,  LPUART3, 3u, kPORT_MuxAlt2},
    {PIN_1_16, PIN_1_17, LPUART5, 5u, kPORT_MuxAlt2},
    {PIN_3_20, PIN_3_21, LPUART6, 6u, kPORT_MuxAlt3},
    {PIN_3_2,  PIN_3_3,  LPUART7, 7u, kPORT_MuxAlt2},
    {PIN_2_4,  PIN_2_3,  LPUART9, 9u, kPORT_MuxAlt2},
};

static uart_state_t states[UART_PORT_COUNT];

static bool port_valid(uart_port_t port)
{
    switch (port) {
    case UART_PORT_0: case UART_PORT_1: case UART_PORT_2: case UART_PORT_3:
    case UART_PORT_5: case UART_PORT_6: case UART_PORT_7: case UART_PORT_9:
        return true;
    default:
        return false;
    }
}

static const uart_route_t *find_route(uart_port_t port)
{
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        if (routes[i].instance == (unsigned)port) return &routes[i];
    return NULL;
}

static bool config_valid(const uart_config_t *config)
{
    return config != NULL && config->baud_rate != 0u
        && (unsigned)config->parity <= UART_PARITY_ODD
        && (unsigned)config->stop_bits <= UART_STOP_BITS_2;
}

hal_error_t uart_init(uart_port_t port, const uart_config_t *config)
{
    if (!port_valid(port) || !config_valid(config))
        return HAL_ERROR_INVALID_ARGUMENT;
    const uart_route_t *const route = find_route(port);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    uart_state_t *const state = &states[port];
    if (state->initialized) return HAL_ERROR_ALREADY_INITIALIZED;
    hal_error_t status = hal_flexcomm_claim(route->instance, HAL_PIN_OWNER_UART);
    if (status != HAL_OK) return status;
    status = hal_pin_claim(route->tx, HAL_PIN_OWNER_UART);
    if (status != HAL_OK) { hal_flexcomm_release(route->instance, HAL_PIN_OWNER_UART); return status; }
    status = hal_pin_claim(route->rx, HAL_PIN_OWNER_UART);
    if (status != HAL_OK) {
        hal_pin_release(route->tx, HAL_PIN_OWNER_UART);
        hal_flexcomm_release(route->instance, HAL_PIN_OWNER_UART);
        return status;
    }
    hal_pin_configure(route->tx, route->mux, GPIO_PULL_NONE, false, true);
    hal_pin_configure(route->rx, route->mux, GPIO_PULL_NONE, false, true);
    const uint32_t clock = hal_flexcomm_enable_clock(route->instance);
    lpuart_config_t sdk;
    LPUART_GetDefaultConfig(&sdk);
    sdk.baudRate_Bps = config->baud_rate;
    sdk.parityMode = config->parity == UART_PARITY_EVEN ? kLPUART_ParityEven
                   : config->parity == UART_PARITY_ODD ? kLPUART_ParityOdd : kLPUART_ParityDisabled;
    sdk.stopBitCount = config->stop_bits == UART_STOP_BITS_2 ? kLPUART_TwoStopBit : kLPUART_OneStopBit;
    sdk.enableTx = true;
    sdk.enableRx = true;
    if (LPUART_Init(route->base, &sdk, clock) != kStatus_Success) {
        hal_pin_make_safe(route->rx); hal_pin_make_safe(route->tx);
        hal_pin_release(route->rx, HAL_PIN_OWNER_UART);
        hal_pin_release(route->tx, HAL_PIN_OWNER_UART);
        hal_flexcomm_release(route->instance, HAL_PIN_OWNER_UART);
        return HAL_ERROR_NOT_SUPPORTED;
    }
    state->route = route;
    state->initialized = true;
    return HAL_OK;
}

hal_error_t uart_deinit(uart_port_t port)
{
    if (!port_valid(port)) return HAL_ERROR_INVALID_ARGUMENT;
    uart_state_t *const state = &states[port];
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    const uart_route_t *const route = state->route;
    LPUART_Deinit(route->base);
    hal_pin_make_safe(route->tx); hal_pin_make_safe(route->rx);
    hal_pin_release(route->tx, HAL_PIN_OWNER_UART);
    hal_pin_release(route->rx, HAL_PIN_OWNER_UART);
    hal_flexcomm_release(route->instance, HAL_PIN_OWNER_UART);
    state->route = NULL;
    state->initialized = false;
    return HAL_OK;
}

hal_error_t uart_write(uart_port_t port, const uint8_t *data, size_t count)
{
    if (!port_valid(port) || (count != 0u && data == NULL))
        return HAL_ERROR_INVALID_ARGUMENT;
    const uart_state_t *const state = &states[port];
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    LPUART_Type *const base = state->route->base;
    size_t sent = 0u;
    while (sent < count) {
        if ((LPUART_GetStatusFlags(base) & kLPUART_TxDataRegEmptyFlag) != 0u)
            LPUART_WriteByte(base, data[sent++]);
    }
    if (count != 0u) {
        while ((LPUART_GetStatusFlags(base) & kLPUART_TransmissionCompleteFlag) == 0u) {
            /* Include the last byte on the wire in the blocking operation. */
        }
    }
    return HAL_OK;
}

hal_error_t uart_available(uart_port_t port, size_t *available)
{
    if (!port_valid(port) || available == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    const uart_state_t *const state = &states[port];
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    *available = LPUART_GetRxFifoCount(state->route->base);
    return HAL_OK;
}

hal_error_t uart_read(uart_port_t port, uint8_t *data, size_t count,
                      size_t *received)
{
    if (!port_valid(port) || received == NULL || (count != 0u && data == NULL))
        return HAL_ERROR_INVALID_ARGUMENT;
    *received = 0u;
    const uart_state_t *const state = &states[port];
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    LPUART_Type *const base = state->route->base;
    while (*received < count) {
        const uint32_t flags = LPUART_GetStatusFlags(base);
        const uint32_t errors = flags & (kLPUART_RxOverrunFlag
            | kLPUART_ParityErrorFlag | kLPUART_FramingErrorFlag
            | kLPUART_NoiseErrorFlag);
        if (errors != 0u) {
            (void)LPUART_ClearStatusFlags(base, errors);
            return HAL_ERROR_IO;
        }
        if ((flags & kLPUART_RxDataRegFullFlag) != 0u)
            data[(*received)++] = LPUART_ReadByte(base);
    }
    return HAL_OK;
}
