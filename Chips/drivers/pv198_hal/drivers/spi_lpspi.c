#include "hal/drivers/spi.h"

#include "internal.h"

#include "fsl_lpspi.h"
#include <stdatomic.h>

typedef struct {
    pin_num_t miso;
    pin_num_t mosi;
    pin_num_t clock;
    pin_num_t chip_select;
    LPSPI_Type *base;
    spi_port_t port;
    port_mux_t miso_mux;
    port_mux_t mosi_mux;
    port_mux_t clock_mux;
    port_mux_t chip_select_mux;
} spi_route_t;

typedef enum {
    SPI_ROLE_NONE,
    SPI_ROLE_MASTER,
    SPI_ROLE_SLAVE,
} spi_role_t;

typedef struct {
    spi_role_t role;
    const spi_route_t *route;
    lpspi_master_handle_t master_handle;
    atomic_bool master_done;
    atomic_int master_status;
    lpspi_slave_handle_t slave_handle;
    spi_slave_isr_t slave_handler;
    void *slave_context;
    size_t slave_capacity;
    atomic_bool slave_active;
} spi_state_t;

static const spi_route_t routes[] = {
    {PIN_0_14, PIN_0_5,  PIN_0_4,  PIN_0_15, LPSPI0, SPI_PORT_0,
     kPORT_MuxAlt3, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_0_26, PIN_0_24, PIN_0_25, PIN_0_27, LPSPI1, SPI_PORT_1,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_4_2,  PIN_4_0,  PIN_4_1,  PIN_4_3,  LPSPI2, SPI_PORT_2,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_1_2,  PIN_1_0,  PIN_1_1,  PIN_1_3,  LPSPI3, SPI_PORT_3,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_1_18, PIN_1_16, PIN_1_17, PIN_1_19, LPSPI5, SPI_PORT_5,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_3_22, PIN_3_20, PIN_3_21, PIN_3_23, LPSPI6, SPI_PORT_6,
     kPORT_MuxAlt3, kPORT_MuxAlt3, kPORT_MuxAlt3, kPORT_MuxAlt1},
    {PIN_3_4,  PIN_3_2,  PIN_3_3,  PIN_3_5,  LPSPI7, SPI_PORT_7,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
    {PIN_2_5,  PIN_2_4,  PIN_2_3,  PIN_2_2,  LPSPI9, SPI_PORT_9,
     kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2, kPORT_MuxAlt2},
};

static spi_state_t states[SPI_PORT_COUNT];

static const spi_route_t *find_route(spi_port_t port)
{
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        if (routes[i].port == port) return &routes[i];
    return NULL;
}

static hal_error_t translate(status_t status)
{
    if (status == kStatus_Success) return HAL_OK;
    if (status == kStatus_LPSPI_Busy) return HAL_ERROR_BUSY;
    return HAL_ERROR_IO;
}

static void master_complete(LPSPI_Type *base,
                            lpspi_master_handle_t *handle,
                            status_t status,
                            void *context)
{
    (void)base;
    (void)handle;
    spi_state_t *state = context;
    atomic_store_explicit(&state->master_status, status,
                          memory_order_relaxed);
    (void)atomic_exchange_explicit(&state->master_done, true,
                                   memory_order_release);
}

static void slave_complete(LPSPI_Type *base,
                           lpspi_slave_handle_t *handle,
                           status_t status,
                           void *context)
{
    (void)base;
    spi_state_t *state = context;
    const size_t transferred = state->slave_capacity
                             - handle->rxRemainingByteCount;
    (void)atomic_exchange_explicit(&state->slave_active, false,
                                   memory_order_release);
    if (state->slave_handler != NULL)
        state->slave_handler(state->route->port, translate(status),
                             transferred, state->slave_context);
}

static bool supports_master(spi_port_t port,
                            const spi_master_config_t *config)
{
    return config != NULL && find_route(port) != NULL;
}

static bool supports_slave(spi_port_t port,
                           const spi_slave_config_t *config)
{
    return config != NULL && find_route(port) != NULL;
}

static hal_error_t claim_pin(pin_num_t pin)
{
    return hal_pin_claim(pin, HAL_PIN_OWNER_SPI);
}

static void release_data_pins(const spi_route_t *route)
{
    hal_pin_make_safe(route->miso);
    hal_pin_make_safe(route->mosi);
    hal_pin_make_safe(route->clock);
    hal_pin_release(route->miso, HAL_PIN_OWNER_SPI);
    hal_pin_release(route->mosi, HAL_PIN_OWNER_SPI);
    hal_pin_release(route->clock, HAL_PIN_OWNER_SPI);
}

static hal_error_t claim_data_pins(const spi_route_t *route)
{
    hal_error_t status = claim_pin(route->miso);
    if (status != HAL_OK) return status;
    status = claim_pin(route->mosi);
    if (status != HAL_OK) {
        hal_pin_release(route->miso, HAL_PIN_OWNER_SPI);
        return status;
    }
    status = claim_pin(route->clock);
    if (status != HAL_OK) {
        hal_pin_release(route->mosi, HAL_PIN_OWNER_SPI);
        hal_pin_release(route->miso, HAL_PIN_OWNER_SPI);
    }
    return status;
}

static void configure_data_pins(const spi_route_t *route)
{
    hal_pin_configure(route->miso, route->miso_mux,
                      GPIO_PULL_NONE, false, true);
    hal_pin_configure(route->mosi, route->mosi_mux,
                      GPIO_PULL_NONE, false, true);
    hal_pin_configure(route->clock, route->clock_mux,
                      GPIO_PULL_NONE, false, true);
}

static void fill_format(lpspi_clock_polarity_t *cpol,
                        lpspi_clock_phase_t *cpha,
                        lpspi_shift_direction_t *direction,
                        spi_clock_polarity_t polarity,
                        spi_clock_phase_t phase,
                        spi_bit_order_t order)
{
    *cpol = polarity == SPI_CLOCK_IDLE_HIGH
          ? kLPSPI_ClockPolarityActiveLow
          : kLPSPI_ClockPolarityActiveHigh;
    *cpha = phase == SPI_CLOCK_PHASE_SECOND_EDGE
          ? kLPSPI_ClockPhaseSecondEdge
          : kLPSPI_ClockPhaseFirstEdge;
    *direction = order == SPI_LSB_FIRST ? kLPSPI_LsbFirst : kLPSPI_MsbFirst;
}

static hal_error_t master_init(spi_port_t port,
                               const spi_master_config_t *config)
{
    const spi_route_t *route = find_route(port);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_NONE) return HAL_ERROR_ALREADY_INITIALIZED;
    hal_error_t status = hal_flexcomm_claim((unsigned)port,
                                             HAL_PIN_OWNER_SPI);
    if (status != HAL_OK) return status;
    status = claim_data_pins(route);
    if (status != HAL_OK) {
        hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_SPI);
        return status;
    }
    configure_data_pins(route);
    lpspi_master_config_t sdk;
    LPSPI_MasterGetDefaultConfig(&sdk);
    sdk.baudRate = config->frequency_hz;
    sdk.bitsPerFrame = 8u;
    fill_format(&sdk.cpol, &sdk.cpha, &sdk.direction,
                config->clock_polarity, config->clock_phase,
                config->bit_order);
    sdk.pinCfg = kLPSPI_SdiInSdoOut;
    LPSPI_MasterInit(route->base, &sdk,
                     hal_flexcomm_enable_clock((unsigned)port));
    state->route = route;
    LPSPI_MasterTransferCreateHandle(route->base, &state->master_handle,
                                     master_complete, state);
    state->role = SPI_ROLE_MASTER;
    return HAL_OK;
}

static hal_error_t master_deinit(spi_port_t port)
{
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_MASTER) return HAL_ERROR_NOT_INITIALIZED;
    LPSPI_Deinit(state->route->base);
    release_data_pins(state->route);
    hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_SPI);
    state->route = NULL;
    state->role = SPI_ROLE_NONE;
    return HAL_OK;
}

static hal_error_t master_transfer(spi_port_t port, const uint8_t *tx,
                                   uint8_t *rx, size_t count)
{
    spi_state_t *state = &states[port];
    lpspi_transfer_t transfer = {
        .txData = (uint8_t *)tx,
        .rxData = rx,
        .dataSize = count,
        .configFlags = kLPSPI_MasterPcs0,
    };
    (void)atomic_exchange_explicit(&state->master_done, false,
                                   memory_order_relaxed);
    status_t status = LPSPI_MasterTransferNonBlocking(state->route->base,
                                                       &state->master_handle,
                                                       &transfer);
    if (status != kStatus_Success) return translate(status);
    while (!atomic_load_explicit(&state->master_done,
                                 memory_order_acquire)) {
        /* Completion/error is published by the peripheral interrupt. */
    }
    return translate((status_t)atomic_load_explicit(&state->master_status,
                                                     memory_order_relaxed));
}

static hal_error_t slave_init(spi_port_t port,
                              const spi_slave_config_t *config)
{
    const spi_route_t *route = find_route(port);
    if (route == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (port == SPI_PORT_6
        && config->clock_polarity == SPI_CLOCK_IDLE_HIGH
        && config->clock_phase == SPI_CLOCK_PHASE_SECOND_EDGE)
        return HAL_ERROR_NOT_SUPPORTED;
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_NONE) return HAL_ERROR_ALREADY_INITIALIZED;
    hal_error_t status = hal_flexcomm_claim((unsigned)port,
                                             HAL_PIN_OWNER_SPI);
    if (status != HAL_OK) return status;
    status = claim_data_pins(route);
    if (status != HAL_OK) {
        hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_SPI);
        return status;
    }
    status = claim_pin(route->chip_select);
    if (status != HAL_OK) {
        release_data_pins(route);
        hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_SPI);
        return status;
    }
    configure_data_pins(route);
    hal_pin_configure(route->chip_select, route->chip_select_mux,
                      GPIO_PULL_UP, false, true);
    lpspi_slave_config_t sdk;
    LPSPI_SlaveGetDefaultConfig(&sdk);
    sdk.bitsPerFrame = 8u;
    fill_format(&sdk.cpol, &sdk.cpha, &sdk.direction,
                config->clock_polarity, config->clock_phase,
                config->bit_order);
    sdk.pinCfg = kLPSPI_SdoInSdiOut;
    (void)hal_flexcomm_enable_clock((unsigned)port);
    LPSPI_SlaveInit(route->base, &sdk);
    state->route = route;
    LPSPI_SlaveTransferCreateHandle(route->base, &state->slave_handle,
                                    slave_complete, state);
    state->role = SPI_ROLE_SLAVE;
    return HAL_OK;
}

static hal_error_t slave_unregister_interrupt(spi_port_t port)
{
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_SLAVE) return HAL_ERROR_NOT_INITIALIZED;
    if (state->slave_handler == NULL) return HAL_ERROR_NOT_INITIALIZED;
    if (atomic_load_explicit(&state->slave_active, memory_order_acquire))
        return HAL_ERROR_BUSY;
    state->slave_handler = NULL;
    state->slave_context = NULL;
    return HAL_OK;
}

static hal_error_t slave_deinit(spi_port_t port)
{
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_SLAVE) return HAL_ERROR_NOT_INITIALIZED;
    if (atomic_load_explicit(&state->slave_active, memory_order_acquire)) {
        LPSPI_SlaveTransferAbort(state->route->base, &state->slave_handle);
        (void)atomic_exchange_explicit(&state->slave_active, false,
                                       memory_order_relaxed);
    }
    LPSPI_Deinit(state->route->base);
    hal_pin_make_safe(state->route->chip_select);
    hal_pin_release(state->route->chip_select, HAL_PIN_OWNER_SPI);
    release_data_pins(state->route);
    hal_flexcomm_release((unsigned)port, HAL_PIN_OWNER_SPI);
    state->route = NULL;
    state->role = SPI_ROLE_NONE;
    return HAL_OK;
}

static hal_error_t slave_transfer(spi_port_t port, const uint8_t *tx,
                                  uint8_t *rx, size_t count)
{
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_SLAVE) return HAL_ERROR_NOT_INITIALIZED;
    if (state->slave_handler == NULL) return HAL_ERROR_NOT_INITIALIZED;
    if (atomic_exchange_explicit(&state->slave_active, true,
                                 memory_order_acq_rel))
        return HAL_ERROR_BUSY;
    state->slave_capacity = count;
    lpspi_transfer_t transfer = {
        .txData = (uint8_t *)tx,
        .rxData = rx,
        .dataSize = count,
        .configFlags = kLPSPI_SlavePcs0,
    };
    status_t status = LPSPI_SlaveTransferNonBlocking(state->route->base,
                                                      &state->slave_handle,
                                                      &transfer);
    if (status != kStatus_Success)
        (void)atomic_exchange_explicit(&state->slave_active, false,
                                       memory_order_release);
    return translate(status);
}

static hal_error_t slave_register_interrupt(spi_port_t port,
                                            spi_slave_isr_t handler,
                                            void *context)
{
    spi_state_t *state = &states[port];
    if (state->role != SPI_ROLE_SLAVE) return HAL_ERROR_NOT_INITIALIZED;
    if (state->slave_handler != NULL) return HAL_ERROR_ALREADY_INITIALIZED;
    state->slave_handler = handler;
    state->slave_context = context;
    return HAL_OK;
}

static const spi_driver_t driver = {
    .supports_master = supports_master,
    .supports_slave = supports_slave,
    .master_init = master_init,
    .master_deinit = master_deinit,
    .master_transfer = master_transfer,
    .slave_init = slave_init,
    .slave_deinit = slave_deinit,
    .slave_transfer = slave_transfer,
    .slave_register_interrupt = slave_register_interrupt,
    .slave_unregister_interrupt = slave_unregister_interrupt,
};

const spi_driver_t *get_lpspi_driver(void)
{
    return &driver;
}
