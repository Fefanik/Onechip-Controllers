#include "hal/pwm.h"
#include "hal/drivers/pwm.h"
#include "internal.h"
#include "fsl_ctimer.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    pin_num_t pin;
    timer_num_t timer;
    ctimer_match_t channel;
    bool initialized;
} pwm_state_t;

/* ALT4 routes from the hardware table in timer.h. One pin per match channel. */
static pwm_state_t outputs[] = {
    {PIN_0_2, TIMER0, kCTIMER_Match_0, false},
    {PIN_0_10, TIMER0, kCTIMER_Match_0, false},
    {PIN_0_16, TIMER0, kCTIMER_Match_0, false},
    {PIN_0_24, TIMER0, kCTIMER_Match_0, false},
    {PIN_0_3, TIMER0, kCTIMER_Match_1, false},
    {PIN_0_11, TIMER0, kCTIMER_Match_1, false},
    {PIN_0_17, TIMER0, kCTIMER_Match_1, false},
    {PIN_0_25, TIMER0, kCTIMER_Match_1, false},
    {PIN_0_5, TIMER0, kCTIMER_Match_3, false},
    {PIN_0_13, TIMER0, kCTIMER_Match_3, false},
    {PIN_0_19, TIMER0, kCTIMER_Match_3, false},
    {PIN_0_27, TIMER0, kCTIMER_Match_3, false},
    {PIN_1_2, TIMER1, kCTIMER_Match_0, false},
    {PIN_3_10, TIMER1, kCTIMER_Match_0, false},
    {PIN_1_3, TIMER1, kCTIMER_Match_1, false},
    {PIN_3_11, TIMER1, kCTIMER_Match_1, false},
    {PIN_1_5, TIMER1, kCTIMER_Match_3, false},
    {PIN_3_13, TIMER1, kCTIMER_Match_3, false},
    {PIN_1_10, TIMER2, kCTIMER_Match_0, false},
    {PIN_3_18, TIMER2, kCTIMER_Match_0, false},
    {PIN_4_20, TIMER2, kCTIMER_Match_0, false},
    {PIN_1_11, TIMER2, kCTIMER_Match_1, false},
    {PIN_3_19, TIMER2, kCTIMER_Match_1, false},
    {PIN_4_21, TIMER2, kCTIMER_Match_1, false},
    {PIN_1_13, TIMER2, kCTIMER_Match_3, false},
    {PIN_3_21, TIMER2, kCTIMER_Match_3, false},
    {PIN_4_23, TIMER2, kCTIMER_Match_3, false},
    {PIN_1_18, TIMER3, kCTIMER_Match_0, false},
    {PIN_4_16, TIMER3, kCTIMER_Match_0, false},
    {PIN_1_19, TIMER3, kCTIMER_Match_1, false},
    {PIN_4_17, TIMER3, kCTIMER_Match_1, false},
    {PIN_1_21, TIMER3, kCTIMER_Match_3, false},
    {PIN_4_19, TIMER3, kCTIMER_Match_3, false},
    {PIN_3_2, TIMER4, kCTIMER_Match_0, false},
    {PIN_4_12, TIMER4, kCTIMER_Match_0, false},
    {PIN_3_3, TIMER4, kCTIMER_Match_1, false},
    {PIN_4_13, TIMER4, kCTIMER_Match_1, false},
    {PIN_3_7, TIMER4, kCTIMER_Match_3, false},
    {PIN_4_15, TIMER4, kCTIMER_Match_3, false},
};
static CTIMER_Type *const bases[] = {CTIMER0, CTIMER1, CTIMER2, CTIMER3, CTIMER4};
static unsigned timer_users[5];

static pwm_state_t *find_output(timer_num_t timer, pin_num_t pin)
{
    for (unsigned i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i)
        if (outputs[i].timer == timer && outputs[i].pin == pin) return &outputs[i];
    return NULL;
}

static bool supports(timer_num_t timer, pin_num_t pin)
{
    return find_output(timer, pin) != NULL;
}

static void setup(pwm_state_t *state, uint32_t period, uint32_t pulse)
{
    CTIMER_Type *const base = bases[state->timer];
    /* TC counts 0..period-1; PWM goes high at its match and low on reset.
     * Exact endpoint levels use the ordinary external-match output, avoiding
     * waiting for a counter reset (the shared timer may be stopped). */
    const uint32_t bit = 1u << state->channel;
    if (pulse == 0u || pulse == period) {
        if (pulse == 0u) base->EMR &= ~bit;
        else base->EMR |= bit;
        base->PWMC &= ~bit;
    } else {
        base->MR[state->channel] = period - pulse;
        base->PWMC |= bit;
    }
}

static hal_error_t init(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks)
{
    pwm_state_t *const state = find_output(timer, pin);
    if (state == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (state->initialized) return HAL_ERROR_ALREADY_INITIALIZED;
    uint32_t period;
    hal_error_t status = hal_timer_require(timer, &period);
    if (status != HAL_OK) return status;
    if (period == TIMER_FREE_RUNNING) return HAL_ERROR_NOT_SUPPORTED;
    if (pulse_ticks > period) return HAL_ERROR_INVALID_ARGUMENT;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    for (unsigned i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i)
        if (outputs[i].initialized && outputs[i].timer == timer
            && outputs[i].channel == state->channel) return HAL_ERROR_BUSY;

    status = hal_pin_claim(pin, HAL_PIN_OWNER_PWM);
    if (status != HAL_OK) return status;
    if (timer_users[timer] == 0u) {
        status = hal_ctimer_claim(timer, HAL_CTIMER_OWNER_PWM);
        if (status != HAL_OK) {
            hal_pin_release(pin, HAL_PIN_OWNER_PWM);
            return status;
        }
    }
    /* Channel setup never touches the timer's prescaler, period or counter. */
    const ctimer_match_config_t match = {
        .outControl = kCTIMER_Output_NoAction,
        .outPinInitState = false,
    };
    CTIMER_SetupMatch(bases[timer], state->channel, &match);
    setup(state, period, pulse_ticks);
    hal_pin_configure(pin, kPORT_MuxAlt4, GPIO_PULL_NONE, false, true);
    ++timer_users[timer];
    state->initialized = true;
    return HAL_OK;
}

static hal_error_t deinit(timer_num_t timer, pin_num_t pin)
{
    pwm_state_t *const state = find_output(timer, pin);
    if (state == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    bases[timer]->PWMC &= ~(1u << state->channel);
    state->initialized = false;
    hal_pin_make_safe(pin);
    hal_pin_release(pin, HAL_PIN_OWNER_PWM);
    if (--timer_users[timer] == 0u) hal_ctimer_release(timer, HAL_CTIMER_OWNER_PWM);
    return HAL_OK;
}

static hal_error_t set_pulse(timer_num_t timer, pin_num_t pin, uint32_t pulse_ticks)
{
    pwm_state_t *const state = find_output(timer, pin);
    if (state == NULL) return HAL_ERROR_NOT_SUPPORTED;
    if (!state->initialized) return HAL_ERROR_NOT_INITIALIZED;
    uint32_t period;
    hal_error_t status = hal_timer_require(timer, &period);
    if (status != HAL_OK) return status;
    if (pulse_ticks > period) return HAL_ERROR_INVALID_ARGUMENT;
    setup(state, period, pulse_ticks);
    return HAL_OK;
}

static hal_error_t stop(timer_num_t timer, pin_num_t pin)
{
    return set_pulse(timer, pin, 0u);
}

static const pwm_driver_t driver = {
    .supports = supports, .init = init, .deinit = deinit,
    .set_pulse = set_pulse, .stop = stop,
};

const pwm_driver_t *get_ctimer_driver(void) { return &driver; }
