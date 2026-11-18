#include "hal/utils.h"

#include "fsl_common.h"

#include <stdatomic.h>

_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2,
               "The HAL requires lock-free C atomic_bool");
_Static_assert(ATOMIC_CHAR_LOCK_FREE == 2,
               "The HAL requires lock-free C byte atomics");
_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "The HAL requires lock-free C integer atomics");

static atomic_uint_least32_t milliseconds;

void SysTick_Handler(void)
{
    (void)atomic_fetch_add_explicit(&milliseconds, 1u, memory_order_relaxed);
}

void hal_utils_init(void)
{
    (void)SysTick_Config(SystemCoreClock / 1000u);
}

hal_error_t delay_ms(uint32_t duration_ms)
{
    const uint32_t start = atomic_load_explicit(&milliseconds, memory_order_relaxed);
    while ((uint32_t)(atomic_load_explicit(&milliseconds, memory_order_relaxed) - start) < duration_ms) {
        __WFE();
    }
    return HAL_OK;
}

irq_state_t critical_enter(void)
{
    return DisableGlobalIRQ();
}

void critical_exit(irq_state_t previous_state)
{
    EnableGlobalIRQ(previous_state);
}
