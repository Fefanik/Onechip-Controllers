#include "hal/timer.h"
#include "internal.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_ctimer.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t period;
    timer_isr_t handler;
    void *context;
    bool initialized;
    bool periodic;
} timer_state_t;

static timer_state_t timers[5];
static CTIMER_Type *const bases[] = {CTIMER0, CTIMER1, CTIMER2, CTIMER3, CTIMER4};
static const IRQn_Type irqs[] = {
    CTIMER0_IRQn, CTIMER1_IRQn, CTIMER2_IRQn, CTIMER3_IRQn, CTIMER4_IRQn,
};

hal_error_t hal_timer_require(unsigned timer, uint32_t *period)
{
    if (timer > TIMER4) return HAL_ERROR_INVALID_ARGUMENT;
    if (!timers[timer].initialized) return HAL_ERROR_NOT_INITIALIZED;
    if (period != NULL) *period = timers[timer].period;
    return HAL_OK;
}

bool hal_timer_running(unsigned timer)
{
    return timer <= TIMER4 && timers[timer].initialized
        && (bases[timer]->TCR & CTIMER_TCR_CEN_MASK) != 0u;
}

static void setup_period(unsigned timer, bool interrupt, bool oneshot)
{
    const ctimer_match_config_t match = {
        .enableCounterReset = timers[timer].period != TIMER_FREE_RUNNING,
        .enableCounterStop = oneshot,
        .matchValue = timers[timer].period - 1u,
        .outControl = kCTIMER_Output_NoAction,
        .outPinInitState = false,
        .enableInterrupt = interrupt,
    };
    CTIMER_SetupMatch(bases[timer], (ctimer_match_t)HAL_TIMER_PERIOD_CHANNEL, &match);
}

hal_error_t timer_init(timer_num_t timer, uint32_t tick_hz, uint32_t period_ticks)
{
    if ((unsigned)timer > TIMER4 || tick_hz == 0u || period_ticks == 1u)
        return HAL_ERROR_INVALID_ARGUMENT;
    if (timers[timer].initialized) return HAL_ERROR_ALREADY_INITIALIZED;

    /* Establish the functional clock BEFORE any CTIMER register access. */
    const clock_div_name_t dividers[] = {kCLOCK_DivCtimer0Clk, kCLOCK_DivCtimer1Clk,
        kCLOCK_DivCtimer2Clk, kCLOCK_DivCtimer3Clk, kCLOCK_DivCtimer4Clk};
    const clock_attach_id_t clocks[] = {kFRO_HF_to_CTIMER0, kFRO_HF_to_CTIMER1,
        kFRO_HF_to_CTIMER2, kFRO_HF_to_CTIMER3, kFRO_HF_to_CTIMER4};
    CLOCK_SetClkDiv(dividers[timer], 1u);
    CLOCK_AttachClk(clocks[timer]);
    const uint32_t clock = CLOCK_GetCTimerClkFreq(timer);
    if (clock < tick_hz || clock % tick_hz != 0u) return HAL_ERROR_NOT_SUPPORTED;

    ctimer_config_t config;
    CTIMER_GetDefaultConfig(&config);
    config.prescale = clock / tick_hz - 1u;
    DisableIRQ(irqs[timer]);
    CTIMER_Init(bases[timer], &config);
    timers[timer] = (timer_state_t){.period = period_ticks, .initialized = true};
    setup_period(timer, false, false);
    CTIMER_Reset(bases[timer]);
    CTIMER_ClearStatusFlags(bases[timer], 0xffu);
    (void)IRQ_ClearPendingIRQ(irqs[timer]);
    return HAL_OK;
}

static void start_counter(unsigned timer)
{
    CTIMER_Reset(bases[timer]);
    CTIMER_ClearStatusFlags(bases[timer], 0xffu);
    (void)IRQ_ClearPendingIRQ(irqs[timer]);
    EnableIRQ(irqs[timer]);
    CTIMER_StartTimer(bases[timer]);
}

hal_error_t timer_start(timer_num_t timer)
{
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    start_counter(timer);
    return HAL_OK;
}

hal_error_t timer_get_count(timer_num_t timer, uint32_t *ticks)
{
    if (ticks == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    *ticks = bases[timer]->TC;
    return HAL_OK;
}

hal_error_t timer_stop(timer_num_t timer)
{
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    DisableIRQ(irqs[timer]);
    CTIMER_StopTimer(bases[timer]);
    if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_TIMER)) {
        timers[timer].handler = NULL;
        timers[timer].context = NULL;
        setup_period(timer, false, false);
        hal_ctimer_release(timer, HAL_CTIMER_OWNER_TIMER);
    }
    CTIMER_ClearStatusFlags(bases[timer], 0xffu);
    (void)IRQ_ClearPendingIRQ(irqs[timer]);
    return HAL_OK;
}

hal_error_t timer_deinit(timer_num_t timer)
{
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_PWM)
        || hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_CAPTURE)) return HAL_ERROR_BUSY;
    (void)timer_stop(timer);
    CTIMER_Deinit(bases[timer]);
    timers[timer] = (timer_state_t){0};
    return HAL_OK;
}

static hal_error_t start_callback(timer_num_t timer, timer_isr_t handler,
                                  void *context, bool periodic)
{
    hal_error_t status = hal_timer_require(timer, NULL);
    if (status != HAL_OK) return status;
    if (handler == NULL || timers[timer].period == TIMER_FREE_RUNNING)
        return HAL_ERROR_INVALID_ARGUMENT;
    if (hal_timer_running(timer)) return HAL_ERROR_BUSY;
    status = hal_ctimer_claim(timer, HAL_CTIMER_OWNER_TIMER);
    if (status != HAL_OK) return status;
    DisableIRQ(irqs[timer]);
    timers[timer].handler = handler;
    timers[timer].context = context;
    timers[timer].periodic = periodic;
    setup_period(timer, true, !periodic);
    start_counter(timer);
    return HAL_OK;
}

hal_error_t timer_start_oneshot(timer_num_t timer, timer_isr_t handler, void *context)
{ return start_callback(timer, handler, context, false); }

hal_error_t timer_start_periodic(timer_num_t timer, timer_isr_t handler, void *context)
{ return start_callback(timer, handler, context, true); }

static void dispatch_irq(timer_num_t timer)
{
    if (!timers[timer].initialized) return;
    if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_CAPTURE)) {
        hal_capture_ctimer_irq(timer);
    } else if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_TIMER)) {
        const uint32_t flag = 1u << HAL_TIMER_PERIOD_CHANNEL;
        if ((CTIMER_GetStatusFlags(bases[timer]) & flag) == 0u) return;
        CTIMER_ClearStatusFlags(bases[timer], flag);
        timer_isr_t handler = timers[timer].handler;
        void *context = timers[timer].context;
        if (!timers[timer].periodic) {
            /* Release before calling the handler so it may rearm the timer. */
            (void)timer_stop(timer);
        }
        if (handler != NULL) handler(timer, context);
    }
}

void CTIMER0_IRQHandler(void) { dispatch_irq(TIMER0); }
void CTIMER1_IRQHandler(void) { dispatch_irq(TIMER1); }
void CTIMER2_IRQHandler(void) { dispatch_irq(TIMER2); }
void CTIMER3_IRQHandler(void) { dispatch_irq(TIMER3); }
void CTIMER4_IRQHandler(void) { dispatch_irq(TIMER4); }
