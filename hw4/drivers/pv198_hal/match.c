#include "hal/timer.h"

#include "internal.h"

#include "fsl_common.h"
#include "fsl_ctimer.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    timer_match_config_t config;
    timer_match_isr_t handler;
    void *context;
    uint32_t interval;
    bool periodic;
    bool oneshot;
} match_state_t;

static match_state_t matches[5][4];
static CTIMER_Type *const bases[] = {CTIMER0, CTIMER1, CTIMER2, CTIMER3, CTIMER4};

static bool valid_match(timer_match_t match)
{
    return (unsigned)match < 4u;
}

static void setup(unsigned timer, timer_match_t match)
{
    const unsigned channel = (unsigned)match;
    const match_state_t *const state = &matches[timer][channel];
    const ctimer_match_config_t config = {
        .enableCounterReset = state->config.reset_counter,
        .enableCounterStop = state->config.stop_timer,
        .matchValue = state->config.count,
        .outControl = kCTIMER_Output_NoAction,
        .outPinInitState = false,
        .enableInterrupt = state->handler != NULL,
    };
    CTIMER_SetupMatch(bases[timer], (ctimer_match_t)channel, &config);
}

hal_error_t timer_match_set(timer_num_t timer, timer_match_t match,
                            const timer_match_config_t *config)
{
    if ((unsigned)timer > TIMER4 || !valid_match(match) || config == NULL)
        return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;

    matches[timer][(unsigned)match].config = *config;
    if (match == TIMER_MATCH2) {
        hal_timer_set_period(timer, config->reset_counter && config->count != UINT32_MAX
                                      ? config->count + 1u
                                      : TIMER_FREE_RUNNING);
    }
    setup(timer, match);
    return HAL_OK;
}

hal_error_t timer_match_set_interrupt(timer_num_t timer, timer_match_t match,
                                timer_match_isr_t handler, void *context)
{
    if ((unsigned)timer > TIMER4 || !valid_match(match))
        return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    match_state_t *const state = &matches[timer][(unsigned)match];
    state->handler = handler;
    state->context = context;
    state->periodic = false;
    state->oneshot = false;
    setup(timer, match);
    return HAL_OK;
}

hal_error_t timer_match_unset_interrupt(timer_num_t timer, timer_match_t match)
{
    return timer_match_set_interrupt(timer, match, NULL, NULL);
}

static hal_error_t start(timer_num_t timer, timer_match_t match,
                         uint32_t offset, timer_match_isr_t handler,
                         void *context, bool periodic)
{
    if ((unsigned)timer > TIMER4 || !valid_match(match)
        || offset == 0u || handler == NULL)
        return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_PWM)
        || hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_CAPTURE)) return HAL_ERROR_BUSY;

    /* Starting restarts the counter at zero, including after a one-shot. */
    uint32_t target = offset - 1u;
    uint32_t period;
    (void)hal_timer_require(timer, &period);
    if (period != TIMER_FREE_RUNNING) target %= period;

    match_state_t *const state = &matches[timer][(unsigned)match];
    timer_match_config_t config = state->config;
    config.count = target;
    config.stop_timer = !periodic;
    status = timer_match_set(timer, match, &config);
    if (status != HAL_OK) return status;
    state->interval = offset;
    state->periodic = periodic;
    state->oneshot = !periodic;
    state->handler = handler;
    state->context = context;
    setup(timer, match);
    return timer_start(timer);
}

hal_error_t timer_match_start_oneshot(timer_num_t timer, timer_match_t match,
                                uint32_t offset_ticks, timer_match_isr_t handler,
                                void *context)
{
    return start(timer, match, offset_ticks, handler, context, false);
}

hal_error_t timer_match_start_periodic(timer_num_t timer, timer_match_t match,
                                 uint32_t interval_ticks, timer_match_isr_t handler,
                                 void *context)
{
    return start(timer, match, interval_ticks, handler, context, true);
}

void hal_match_ctimer_irq(unsigned timer)
{
    if (timer > TIMER4) return;
    const uint32_t flags = CTIMER_GetStatusFlags(bases[timer]);
    for (unsigned channel = 0; channel < 4u; ++channel) {
        match_state_t *const state = &matches[timer][channel];
        const uint32_t flag = 1u << channel;
        if (state->handler == NULL || (flags & flag) == 0u) continue;
        CTIMER_ClearStatusFlags(bases[timer], flag);
        timer_match_isr_t handler = state->handler;
        void *context = state->context;
        if (state->periodic) {
            uint32_t next = state->config.count + state->interval;
            uint32_t period;
            (void)hal_timer_require(timer, &period);
            if (period != TIMER_FREE_RUNNING) next %= period;
            state->config.count = next;
            setup(timer, (timer_match_t)channel);
        } else if (state->oneshot) {
            state->handler = NULL;
            setup(timer, (timer_match_t)channel);
        }
        handler((timer_num_t)timer, context);
    }
}

void hal_match_stop(unsigned timer)
{
    if (timer > TIMER4) return;
    for (unsigned channel = 0; channel < 4u; ++channel) {
        const bool had_handler = matches[timer][channel].handler != NULL;
        matches[timer][channel].handler = NULL;
        matches[timer][channel].context = NULL;
        matches[timer][channel].periodic = false;
        matches[timer][channel].oneshot = false;
        /* PWM owns its own match registers; stopping callbacks must not
         * replace those registers with this component's default state. */
        if (had_handler) setup(timer, (timer_match_t)channel);
    }
}

void hal_match_deinit(unsigned timer)
{
    if (timer > TIMER4) return;
    for (unsigned channel = 0; channel < 4u; ++channel)
        matches[timer][channel] = (match_state_t){0};
}
