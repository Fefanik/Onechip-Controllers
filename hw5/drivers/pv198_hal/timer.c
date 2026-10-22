#include "hal/timer.h"

#include "internal.h"

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_ctimer.h"

#include <stdbool.h>


typedef struct {
    uint32_t period;
    bool initialized;
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

void hal_timer_set_period(unsigned timer, uint32_t period)
{
    if (timer <= TIMER4 && timers[timer].initialized)
        timers[timer].period = period;
}

bool hal_timer_running(unsigned timer)
{
    return timer <= TIMER4 && timers[timer].initialized
        && (bases[timer]->TCR & CTIMER_TCR_CEN_MASK) != 0u;
}

hal_error_t timer_init(timer_num_t timer, uint32_t tick_hz)
{
    if ((unsigned)timer > TIMER4 || tick_hz == 0u)
        return HAL_ERROR_INVALID_ARGUMENT;
    if (timers[timer].initialized) return HAL_ERROR_ALREADY_INITIALIZED;

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
    timers[timer] = (timer_state_t){.period = TIMER_FREE_RUNNING, .initialized = true};
    CTIMER_Reset(bases[timer]);
    CTIMER_ClearStatusFlags(bases[timer], 0xffu);
    (void)IRQ_ClearPendingIRQ(irqs[timer]);
    return HAL_OK;
}

hal_error_t timer_init_period(timer_num_t timer, uint32_t tick_hz,
                              uint32_t period_ticks)
{
    if (period_ticks == 1u) return HAL_ERROR_INVALID_ARGUMENT;
    hal_error_t status = timer_init(timer, tick_hz);
    if (status != HAL_OK) return status;
    const timer_match_config_t config = {
        .count = period_ticks == TIMER_FREE_RUNNING ? UINT32_MAX : period_ticks - 1u,
        .reset_counter = period_ticks != TIMER_FREE_RUNNING,
        .stop_timer = false,
    };
    status = timer_match_set(timer, TIMER_MATCH2, &config);
    if (status != HAL_OK) (void)timer_deinit(timer);
    return status;
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
    hal_match_stop(timer);
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
    hal_match_deinit(timer);
    CTIMER_Deinit(bases[timer]);
    timers[timer] = (timer_state_t){0};
    return HAL_OK;
}

static void dispatch_irq(timer_num_t timer)
{
    if (!timers[timer].initialized) return;
    if (hal_ctimer_owned_by(timer, HAL_CTIMER_OWNER_CAPTURE))
        hal_capture_ctimer_irq(timer);
    hal_match_ctimer_irq(timer);
}

void CTIMER0_IRQHandler(void) { dispatch_irq(TIMER0); }
void CTIMER1_IRQHandler(void) { dispatch_irq(TIMER1); }
void CTIMER2_IRQHandler(void) { dispatch_irq(TIMER2); }
void CTIMER3_IRQHandler(void) { dispatch_irq(TIMER3); }
void CTIMER4_IRQHandler(void) { dispatch_irq(TIMER4); }
