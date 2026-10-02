#include "hal/drivers/i2c.h"

#include "internal.h"

#include "fsl_lpi2c.h"

#include <stdatomic.h>

typedef struct {
    pin_num_t sda;
    pin_num_t scl;
    LPI2C_Type *base;
    i2c_port_t port;
    port_mux_t mux;
} i2c_route_t;

typedef struct {
    bool initialized;
    i2c_config_t config;
    const i2c_route_t *route;
    LPI2C_Type *base;
    lpi2c_master_handle_t handle;
    atomic_bool transfer_done;
    atomic_int transfer_status;
} i2c_state_t;

static const i2c_route_t routes[] = {
    {PIN_0_12, PIN_0_13, LPI2C0, I2C_PORT_0, kPORT_MuxAlt3},
    {PIN_0_24, PIN_0_25, LPI2C1, I2C_PORT_1, kPORT_MuxAlt2},
    {PIN_4_0,  PIN_4_1,  LPI2C2, I2C_PORT_2, kPORT_MuxAlt2},
    {PIN_1_0,  PIN_1_1,  LPI2C3, I2C_PORT_3, kPORT_MuxAlt2},
    {PIN_1_16, PIN_1_17, LPI2C5, I2C_PORT_5, kPORT_MuxAlt2},
    {PIN_3_20, PIN_3_21, LPI2C6, I2C_PORT_6, kPORT_MuxAlt3},
    {PIN_3_2,  PIN_3_3,  LPI2C7, I2C_PORT_7, kPORT_MuxAlt2},
    {PIN_2_4,  PIN_2_3,  LPI2C9, I2C_PORT_9, kPORT_MuxAlt2},
};

static i2c_state_t states[I2C_PORT_COUNT];

static const i2c_route_t *find_route(i2c_port_t port)
{
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        if (routes[i].port == port) return &routes[i];
    return NULL;
}

static void complete(LPI2C_Type *base, lpi2c_master_handle_t *handle,
                     status_t status, void *context)
{
    (void)base;
    (void)handle;
    i2c_state_t *state = context;
    atomic_store_explicit(&state->transfer_status, status,
                          memory_order_relaxed);
    (void)atomic_exchange_explicit(&state->transfer_done, true,
                                   memory_order_release);
}

static hal_error_t translate(status_t status)
{
    if (status == kStatus_Success) return HAL_OK;
    if (status == kStatus_LPI2C_Nak) return HAL_ERROR_NACK;
    if (status == kStatus_LPI2C_Busy) return HAL_ERROR_BUSY;
    if (status == kStatus_LPI2C_Timeout || status == kStatus_LPI2C_PinLowTimeout)
        return HAL_ERROR_TIMEOUT;
    return HAL_ERROR_IO;
}

static hal_error_t run(i2c_state_t *state,
                       lpi2c_master_transfer_t *transfer)
{
    (void)atomic_exchange_explicit(&state->transfer_done, false,
                                   memory_order_relaxed);
    status_t status = LPI2C_MasterTransferNonBlocking(state->base,
                                                       &state->handle,
                                                       transfer);
    if (status != kStatus_Success) return translate(status);
    while (!atomic_load_explicit(&state->transfer_done,
                                 memory_order_acquire)) {
        /* Completion/error is published by the peripheral interrupt. */
    }
    return translate((status_t)atomic_load_explicit(&state->transfer_status,
                                                     memory_order_relaxed));
}

static bool supports(i2c_port_t port, const i2c_config_t *config)
{
    return config != NULL && find_route(port) != NULL;
}

static hal_error_t init(i2c_port_t port, const i2c_config_t *config)
{
    const i2c_route_t *route = find_route(port);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    i2c_state_t *state = &states[port];
    if (state->initialized) return HAL_ERROR_ALREADY_INITIALIZED;
    hal_error_t status = hal_flexcomm_claim((unsigned)port,
                                             HAL_PIN_OWNER_I2C);
    if (status != HAL_OK) return status;
    status = hal_pin_claim(route->sda, HAL_PIN_OWNER_I2C);
    if (status != HAL_OK) {
        hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_I2C);
        return status;
    }
    status = hal_pin_claim(route->scl, HAL_PIN_OWNER_I2C);
    if (status != HAL_OK) {
        hal_pin_release(route->sda, HAL_PIN_OWNER_I2C);
        hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_I2C);
        return status;
    }
    hal_pin_configure(route->sda, route->mux, GPIO_PULL_UP, false, true);
    hal_pin_configure(route->scl, route->mux, GPIO_PULL_UP, false, true);
    const uint32_t clock = hal_flexcomm_enable_clock((unsigned)port);
    lpi2c_master_config_t sdk;
    LPI2C_MasterGetDefaultConfig(&sdk);
    sdk.baudRate_Hz = config->frequency_hz;
    LPI2C_MasterInit(route->base, &sdk, clock);
    state->config = *config;
    state->route = route;
    state->base = route->base;
    LPI2C_MasterTransferCreateHandle(route->base, &state->handle,
                                     complete, state);
    state->initialized = true;
    return HAL_OK;
}

static hal_error_t deinit(i2c_port_t port)
{
    i2c_state_t *state = &states[port];
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    LPI2C_MasterDeinit(state->base);
    hal_pin_make_safe(state->route->sda);
    hal_pin_make_safe(state->route->scl);
    hal_pin_release(state->route->sda, HAL_PIN_OWNER_I2C);
    hal_pin_release(state->route->scl, HAL_PIN_OWNER_I2C);
    hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_I2C);
    state->base = NULL;
    state->route = NULL;
    state->initialized = false;
    return HAL_OK;
}

static hal_error_t read(i2c_port_t port, uint8_t address, uint8_t *buffer,
                        size_t count)
{
    i2c_state_t *state = &states[port];
    lpi2c_master_transfer_t transfer = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction = kLPI2C_Read,
        .data = buffer,
        .dataSize = count,
    };
    return run(state, &transfer);
}

static hal_error_t write(i2c_port_t port, uint8_t address,
                         const uint8_t *buffer, size_t count)
{
    i2c_state_t *state = &states[port];
    lpi2c_master_transfer_t transfer = {
        .flags = kLPI2C_TransferDefaultFlag,
        .slaveAddress = address,
        .direction = kLPI2C_Write,
        .data = (void *)buffer,
        .dataSize = count,
    };
    return run(state, &transfer);
}

static hal_error_t write_read(i2c_port_t port, uint8_t address,
                              const uint8_t *write_buffer,
                              size_t write_count,
                              uint8_t *read_buffer,
                              size_t read_count)
{
    i2c_state_t *state = &states[port];
    lpi2c_master_transfer_t write_transfer = {
        .flags = kLPI2C_TransferNoStopFlag,
        .slaveAddress = address,
        .direction = kLPI2C_Write,
        .data = (void *)write_buffer,
        .dataSize = write_count,
    };
    hal_error_t status = run(state, &write_transfer);
    if (status != HAL_OK) return status;
    lpi2c_master_transfer_t read_transfer = {
        .flags = kLPI2C_TransferRepeatedStartFlag,
        .slaveAddress = address,
        .direction = kLPI2C_Read,
        .data = read_buffer,
        .dataSize = read_count,
    };
    return run(state, &read_transfer);
}

static const i2c_driver_t driver = {
    .supports = supports,
    .init = init,
    .deinit = deinit,
    .read = read,
    .write = write,
    .write_read = write_read,
};

const i2c_driver_t *get_lpi2c_driver(void)
{
    return &driver;
}
