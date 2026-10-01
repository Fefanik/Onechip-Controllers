#include "hal/capture.h"

#include "hal/drivers/capture.h"

#include "internal.h"

#include "fsl_common.h"
#include "fsl_ctimer.h"
#include "fsl_gpio.h"
#include "fsl_inputmux.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CAPTURE_TIMER_COUNT = 5,
    CAPTURE_CHANNEL_COUNT = 4,
};

typedef struct {
    pin_num_t pin;
    uint8_t input;
} capture_route_t;

typedef struct {
    capture_isr_t handler;
    void *context;
    pin_num_t pin;
    capture_edge_t edge;
} capture_state_t;

/* All MCXN947 CT_INP pin functions use ALT4. Several pins can expose the
 * same CT_INP signal, but only one of those alternatives may be active. */
static const capture_route_t routes[] = {
    {PIN_0_0, 0},   {PIN_0_1, 1},   {PIN_0_6, 2},   {PIN_0_7, 3},
    {PIN_0_8, 0},   {PIN_0_9, 1},   {PIN_0_14, 2},  {PIN_0_15, 3},
    {PIN_0_20, 0},  {PIN_0_21, 1},  {PIN_0_22, 2},  {PIN_0_23, 3},
    {PIN_1_0, 4},   {PIN_1_1, 5},   {PIN_1_6, 6},   {PIN_1_7, 7},
    {PIN_1_8, 8},   {PIN_1_9, 9},   {PIN_1_14, 10}, {PIN_1_15, 11},
    {PIN_1_16, 12}, {PIN_1_17, 13}, {PIN_1_30, 16}, {PIN_1_31, 17},
    {PIN_3_0, 16},  {PIN_3_1, 17},  {PIN_3_4, 18},  {PIN_3_5, 19},
    {PIN_3_8, 4},   {PIN_3_9, 5},   {PIN_3_14, 6},  {PIN_3_15, 7},
    {PIN_3_16, 8},  {PIN_3_17, 9},  {PIN_3_22, 10}, {PIN_3_23, 11},
    {PIN_4_0, 16},  {PIN_4_1, 17},  {PIN_4_2, 12},  {PIN_4_3, 13},
    {PIN_4_4, 14},  {PIN_4_5, 15},  {PIN_4_6, 18},  {PIN_4_7, 19},
};

static capture_state_t captures[CAPTURE_TIMER_COUNT][CAPTURE_CHANNEL_COUNT];
static unsigned capture_users[CAPTURE_TIMER_COUNT];
static CTIMER_Type *const bases[] = {CTIMER0, CTIMER1, CTIMER2, CTIMER3, CTIMER4};
static const IRQn_Type irqs[] = {
    CTIMER0_IRQn, CTIMER1_IRQn, CTIMER2_IRQn, CTIMER3_IRQn, CTIMER4_IRQn,
};
static const inputmux_connection_t inputs[] = {
    kINPUTMUX_CtimerInp0ToTimer0Captsel, kINPUTMUX_CtimerInp0ToTimer1Captsel,
    kINPUTMUX_CtimerInp0ToTimer2Captsel, kINPUTMUX_CtimerInp0ToTimer3Captsel,
    kINPUTMUX_CtimerInp0ToTimer4Captsel,
};

static const capture_route_t *find_route(pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
        if (routes[i].pin == pin) return &routes[i];
    return NULL;
}

static capture_state_t *find_capture(unsigned timer, pin_num_t pin)
{
    for (unsigned i = 0; i < CAPTURE_CHANNEL_COUNT; ++i)
        if (captures[timer][i].handler != NULL && captures[timer][i].pin == pin)
            return &captures[timer][i];
    return NULL;
}

static ctimer_capture_edge_t hardware_edge(capture_edge_t edge)
{
    return edge == CAPTURE_RISING ? kCTIMER_Capture_RiseEdge
         : edge == CAPTURE_FALLING ? kCTIMER_Capture_FallEdge
                                   : kCTIMER_Capture_BothEdge;
}

static hal_error_t init(timer_num_t timer, pin_num_t pin, capture_edge_t edge,
                        capture_isr_t handler, void *context)
{
    if ((unsigned)timer >= CAPTURE_TIMER_COUNT || !hal_pin_valid(pin)
        || handler == NULL || (unsigned)edge > CAPTURE_ANY)
        return HAL_ERROR_INVALID_ARGUMENT;
    const capture_route_t *const route = find_route(pin);
    if (route == NULL || route->input / CAPTURE_CHANNEL_COUNT != (unsigned)timer)
        return HAL_ERROR_NOT_SUPPORTED;
    const unsigned channel = route->input % CAPTURE_CHANNEL_COUNT;
    capture_state_t *const state = &captures[timer][channel];
    if (state->handler != NULL)
        return state->pin == pin ? HAL_ERROR_ALREADY_INITIALIZED : HAL_ERROR_BUSY;

    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    status = hal_pin_claim(pin, HAL_PIN_OWNER_CAPTURE);
    if (status != HAL_OK) return status;
    if (capture_users[timer] == 0u) {
        status = hal_ctimer_claim(timer, HAL_CTIMER_OWNER_CAPTURE);
        if (status != HAL_OK) {
            hal_pin_release(pin, HAL_PIN_OWNER_CAPTURE);
            return status;
        }
    }

    const uint32_t primask = DisableGlobalIRQ();
    INPUTMUX_Init(INPUTMUX0);
    hal_pin_configure(pin, kPORT_MuxAlt4, GPIO_PULL_NONE, false, true);
    INPUTMUX_AttachSignal(
        INPUTMUX0, channel,
        (inputmux_connection_t)(inputs[timer] + route->input));
    *state = (capture_state_t){handler, context, pin, edge};
    ++capture_users[timer];
    CTIMER_SetupCapture(bases[timer], (ctimer_capture_channel_t)channel,
                        hardware_edge(edge), true);
    /* SDK setup enables NVIC; channel configuration must not deliver callbacks
     * until the student explicitly starts the shared counter. */
    DisableIRQ(irqs[timer]);
    CTIMER_ClearStatusFlags(bases[timer], (uint32_t)kCTIMER_Capture0Flag << channel);
    EnableGlobalIRQ(primask);
    return HAL_OK;
}

static hal_error_t deinit(timer_num_t timer, pin_num_t pin)
{
    if ((unsigned)timer >= CAPTURE_TIMER_COUNT || !hal_pin_valid(pin))
        return HAL_ERROR_INVALID_ARGUMENT;
    const capture_route_t *const route = find_route(pin);
    if (route == NULL || route->input / CAPTURE_CHANNEL_COUNT != (unsigned)timer)
        return HAL_ERROR_NOT_SUPPORTED;
    capture_state_t *const state = find_capture(timer, pin);
    if (state == NULL) return HAL_ERROR_NOT_INITIALIZED;

    const uint32_t primask = DisableGlobalIRQ();
    const unsigned channel = (unsigned)(state - captures[timer]);
    const uint32_t channel_mask =
        (CTIMER_CCR_CAP0RE_MASK | CTIMER_CCR_CAP0FE_MASK
         | CTIMER_CCR_CAP0I_MASK) << (channel * 3u);
    bases[timer]->CCR &= ~channel_mask;
    CTIMER_ClearStatusFlags(bases[timer],
                            (uint32_t)kCTIMER_Capture0Flag << channel);
    *state = (capture_state_t){0};
    --capture_users[timer];
    hal_pin_make_safe(pin);
    hal_pin_release(pin, HAL_PIN_OWNER_CAPTURE);

    if (capture_users[timer] == 0u) {
        DisableIRQ(irqs[timer]);
        hal_ctimer_release(timer, HAL_CTIMER_OWNER_CAPTURE);
    }
    EnableGlobalIRQ(primask);
    return HAL_OK;
}

void hal_capture_ctimer_irq(unsigned timer)
{
    if (!hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_CAPTURE)) return;
    const uint32_t flags = CTIMER_GetStatusFlags(bases[timer]);
    for (unsigned channel = 0; channel < CAPTURE_CHANNEL_COUNT; ++channel) {
        const uint32_t flag = (uint32_t)kCTIMER_Capture0Flag << channel;
        if ((flags & flag) == 0u) continue;

        const uint32_t timestamp = CTIMER_GetCaptureValue(
            bases[timer], (ctimer_capture_channel_t)channel);
        CTIMER_ClearStatusFlags(bases[timer], flag);

        const capture_state_t state = captures[timer][channel];
        if (state.handler == NULL) continue;
        capture_edge_t edge = state.edge;
        if (edge == CAPTURE_ANY) {
            GPIO_Type *const gpio = hal_gpio_base(PIN_PORT(state.pin));
            edge = GPIO_PinRead(gpio, PIN_INDEX(state.pin)) != 0u
                 ? CAPTURE_RISING : CAPTURE_FALLING;
        }
        state.handler(state.pin, edge, timestamp, state.context);
    }
}

static bool supports(timer_num_t timer, pin_num_t pin)
{
    const capture_route_t *const route = find_route(pin);
    return (unsigned)timer < CAPTURE_TIMER_COUNT && route != NULL
        && route->input / CAPTURE_CHANNEL_COUNT == (unsigned)timer;
}

static const capture_driver_t driver = {
    .supports = supports,
    .init = init,
    .deinit = deinit,
};

const capture_driver_t *get_ctimer_capture_driver(void)
{
    return &driver;
}
