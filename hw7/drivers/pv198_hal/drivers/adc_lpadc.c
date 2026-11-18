#include "hal/adc.h"
#include "hal/drivers/adc.h"

#include "internal.h"

#include "fsl_clock.h"
#include "fsl_lpadc.h"
#include "fsl_spc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    pin_num_t pin;
    ADC_Type *base;
    uint32_t channel;
    unsigned instance;
    lpadc_sample_channel_mode_t mode;
} adc_channel_t;

static bool initialized_pins[PIN_MAX];
static unsigned adc_users[2];
static bool analog_reference_initialized;

typedef struct {
    adc_isr_t active_handler;
    void *active_context;
    pin_num_t active_pin;
    atomic_bool busy;
} adc_state_t;

static adc_state_t states[2];
static const IRQn_Type irqs[] = {ADC0_IRQn, ADC1_IRQn};

/* One asynchronous request per initialized facade binding, including the
 * active request at the head. PRIMASK protects queue/pending mutations. The
 * global reservation also covers blocking reads and callback dispatch, so
 * callback rearming can enqueue but cannot start a second hardware stream. */
enum { ADC_REQUEST_CAPACITY = 8 };
typedef struct {
    adc_channel_t adc;
    adc_isr_t handler;
    void *context;
} adc_request_t;
static adc_request_t requests[ADC_REQUEST_CAPACITY];
static unsigned request_head;
static unsigned request_count;
static bool pending_pins[PIN_MAX];
static atomic_bool conversion_busy;

static const adc_channel_t channels[] = {
#define ADC_A(pin_, instance_, channel_) \
    {pin_, ADC##instance_, channel_, instance_, kLPADC_SampleChannelSingleEndSideA}
#define ADC_B(pin_, instance_, channel_) \
    {pin_, ADC##instance_, channel_, instance_, kLPADC_SampleChannelSingleEndSideB}
    /* Dedicated analog pads have no PORT/GPIO mux. Shared pads use one
     * canonical converter so pin ownership and callbacks stay unambiguous. */
    ADC_A(PIN_ANA_0, 0, 0), ADC_B(PIN_ANA_1, 0, 0),
    ADC_A(PIN_ANA_4, 1, 0), ADC_B(PIN_ANA_5, 1, 0),
    ADC_A(PIN_ANA_6, 0, 3), ADC_A(PIN_ANA_7, 0, 7),
    ADC_B(PIN_0_8, 0, 8), ADC_B(PIN_0_9, 0, 9),
    ADC_B(PIN_0_10, 0, 10), ADC_B(PIN_0_11, 0, 11),
    ADC_B(PIN_0_12, 0, 12), ADC_B(PIN_0_13, 0, 13),
    ADC_B(PIN_0_14, 0, 14), ADC_B(PIN_0_15, 0, 15),
    ADC_A(PIN_0_16, 0, 8), ADC_A(PIN_0_17, 0, 9),
    ADC_A(PIN_0_18, 0, 10), ADC_A(PIN_0_19, 0, 11),
    ADC_A(PIN_0_20, 0, 12), ADC_A(PIN_0_21, 0, 13),
    ADC_A(PIN_0_22, 0, 14), ADC_A(PIN_0_23, 0, 15),
    ADC_B(PIN_0_24, 0, 16), ADC_B(PIN_0_25, 0, 17),
    ADC_B(PIN_0_26, 0, 18), ADC_B(PIN_0_27, 0, 19),
    ADC_B(PIN_0_28, 0, 20), ADC_B(PIN_0_29, 0, 21),
    ADC_B(PIN_0_30, 0, 22), ADC_B(PIN_0_31, 0, 23),
    ADC_A(PIN_1_0, 0, 16), ADC_A(PIN_1_1, 0, 17),
    ADC_A(PIN_1_2, 0, 18), ADC_A(PIN_1_3, 0, 19),
    ADC_A(PIN_1_4, 0, 20), ADC_A(PIN_1_5, 0, 21),
    ADC_A(PIN_1_6, 0, 22), ADC_A(PIN_1_7, 0, 23),
    ADC_A(PIN_1_8, 1, 8), ADC_A(PIN_1_9, 1, 9),
    ADC_A(PIN_1_10, 1, 10), ADC_A(PIN_1_11, 1, 11),
    ADC_A(PIN_1_12, 1, 12), ADC_A(PIN_1_13, 1, 13),
    ADC_A(PIN_1_14, 1, 14), ADC_A(PIN_1_15, 1, 15),
    ADC_A(PIN_1_16, 1, 16), ADC_A(PIN_1_17, 1, 17),
    ADC_A(PIN_1_18, 1, 18), ADC_A(PIN_1_19, 1, 19),
    ADC_A(PIN_1_20, 1, 20), ADC_A(PIN_1_21, 1, 21),
    ADC_A(PIN_1_22, 1, 22), ADC_A(PIN_1_23, 1, 23),
    ADC_A(PIN_4_2, 0, 4), ADC_B(PIN_4_3, 0, 4),
    ADC_A(PIN_4_12, 0, 5), ADC_B(PIN_4_13, 0, 5),
    ADC_A(PIN_4_15, 0, 1), ADC_A(PIN_4_16, 0, 6),
    ADC_B(PIN_4_17, 0, 6), ADC_B(PIN_4_19, 0, 1),
    ADC_A(PIN_4_20, 1, 6), ADC_B(PIN_4_21, 1, 6),
    ADC_A(PIN_4_23, 0, 2),
    ADC_B(PIN_5_0, 1, 8), ADC_B(PIN_5_1, 1, 9),
    ADC_B(PIN_5_2, 1, 10), ADC_B(PIN_5_3, 1, 11),
    ADC_B(PIN_5_4, 1, 12), ADC_B(PIN_5_5, 1, 13),
    ADC_B(PIN_5_6, 1, 14), ADC_B(PIN_5_7, 1, 15),
    ADC_B(PIN_5_8, 1, 16), ADC_B(PIN_5_9, 1, 17),
#undef ADC_A
#undef ADC_B
};

static bool adc_channel(pin_num_t pin, adc_channel_t *adc)
{
    for (unsigned i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i) {
        if (channels[i].pin != pin) continue;
        *adc = channels[i];
        return true;
    }
    return false;
}

static bool supports(pin_num_t pin)
{
    adc_channel_t adc;
    return adc_channel(pin, &adc);
}

static void init_converter(const adc_channel_t *adc)
{
    if (!analog_reference_initialized) {
        /* LPADC requires the VREF low-power buffer as its bias-current source. */
        SPC_EnableActiveModeAnalogModules(SPC0, kSPC_controlVref);
        CLOCK_EnableClock(kCLOCK_Vref);
        VREF0->CSR |= VREF_CSR_LPBGEN_MASK;
        SDK_DelayAtLeastUs(20u, SystemCoreClock);
        VREF0->CSR |= VREF_CSR_LPBG_BUF_EN_MASK;
        analog_reference_initialized = true;
    }
    if (adc->instance == 0u) {
        CLOCK_AttachClk(kFRO_HF_to_ADC0);
        CLOCK_SetClkDiv(kCLOCK_DivAdc0Clk, 1u);
    } else {
        CLOCK_AttachClk(kFRO_HF_to_ADC1);
        CLOCK_SetClkDiv(kCLOCK_DivAdc1Clk, 1u);
    }
    lpadc_config_t config;
    LPADC_GetDefaultConfig(&config);
    /* FRO_HF requires the converter's highest analog power setting. */
    config.powerLevelMode = kLPADC_PowerLevelAlt4;
    config.enableAnalogPreliminary = true;
    config.referenceVoltageSource = kLPADC_ReferenceVoltageAlt3;
    LPADC_Init(adc->base, &config);
    LPADC_DoOffsetCalibration(adc->base);
    LPADC_DoAutoCalibration(adc->base);
}

static void configure_conversion(const adc_channel_t *adc)
{
    lpadc_conv_command_config_t command;
    LPADC_GetDefaultConvCommandConfig(&command);
    command.channelNumber = adc->channel;
    command.sampleChannelMode = adc->mode;
    command.conversionResolutionMode = kLPADC_ConversionResolutionHigh;
    LPADC_SetConvCommandConfig(adc->base, 1u, &command);

    lpadc_conv_trigger_config_t trigger;
    LPADC_GetDefaultConvTriggerConfig(&trigger);
    trigger.targetCommandId = 1u;
    LPADC_SetConvTriggerConfig(adc->base, 0u, &trigger);
}

/* Called with interrupts masked and the global conversion reservation held.
 * No callback is invoked here. A request stays at the head until completion. */
static void dispatch_next(void)
{
    if (request_count == 0u) {
        atomic_store_explicit(&conversion_busy, false, memory_order_release);
        return;
    }
    const adc_request_t *const request = &requests[request_head];
    const adc_channel_t *const adc = &request->adc;
    adc_state_t *const state = &states[adc->instance];
    atomic_store_explicit(&state->busy, true, memory_order_release);
    state->active_pin = adc->pin;
    state->active_handler = request->handler;
    state->active_context = request->context;
    configure_conversion(adc);
    LPADC_DoResetFIFO0(adc->base);
    (void)IRQ_ClearPendingIRQ(irqs[adc->instance]);
    EnableIRQ(irqs[adc->instance]);
    LPADC_EnableInterrupts(adc->base, kLPADC_FIFO0WatermarkInterruptEnable);
    LPADC_DoSoftwareTrigger(adc->base, 1u);
}

static hal_error_t init(pin_num_t pin)
{
    adc_channel_t adc;
    if (!adc_channel(pin, &adc)) return HAL_ERROR_NOT_SUPPORTED;
    if (initialized_pins[(unsigned)pin])
        return HAL_ERROR_ALREADY_INITIALIZED;
    hal_error_t status = hal_pin_claim(pin, HAL_PIN_OWNER_ADC);
    if (status != HAL_OK) return status;
    if (adc_users[adc.instance] == 0u) init_converter(&adc);
    ++adc_users[adc.instance];
    initialized_pins[(unsigned)pin] = true;
    if (hal_pin_valid(pin))
        hal_pin_configure(pin, kPORT_MuxAlt0, GPIO_PULL_NONE, false, false);
    return HAL_OK;
}

static hal_error_t deinit(pin_num_t pin)
{
    adc_channel_t adc;
    if (!adc_channel(pin, &adc)) return HAL_ERROR_NOT_SUPPORTED;
    const uint32_t saved = DisableGlobalIRQ();
    if (!initialized_pins[(unsigned)pin]) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_NOT_INITIALIZED;
    }
    if (pending_pins[(unsigned)pin]
        || atomic_load_explicit(&states[adc.instance].busy, memory_order_acquire)) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_BUSY;
    }
    initialized_pins[(unsigned)pin] = false;
    if (--adc_users[adc.instance] == 0u) {
        DisableIRQ(irqs[adc.instance]);
        LPADC_Deinit(adc.base);
    }
    if (hal_pin_valid(pin)) hal_pin_make_safe(pin);
    hal_pin_release(pin, HAL_PIN_OWNER_ADC);
    EnableGlobalIRQ(saved);
    return HAL_OK;
}

static hal_error_t read_raw(pin_num_t pin, uint16_t *sample)
{
    adc_channel_t adc;
    if (sample == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    if (!adc_channel(pin, &adc)) return HAL_ERROR_NOT_SUPPORTED;
    const uint32_t saved = DisableGlobalIRQ();
    if (!initialized_pins[(unsigned)pin]) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_NOT_INITIALIZED;
    }
    adc_state_t *state = &states[adc.instance];
    if (atomic_exchange_explicit(&conversion_busy, true, memory_order_acquire)) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_BUSY;
    }
    atomic_store_explicit(&state->busy, true, memory_order_release);
    pending_pins[(unsigned)pin] = true;
    LPADC_DisableInterrupts(adc.base, kLPADC_FIFO0WatermarkInterruptEnable);
    configure_conversion(&adc);
    LPADC_DoResetFIFO0(adc.base);
    LPADC_DoSoftwareTrigger(adc.base, 1u);
    EnableGlobalIRQ(saved);
    lpadc_conv_result_t result;
    LPADC_GetConvResultBlocking(adc.base, &result, 0u);
    *sample = result.convValue;
    const uint32_t finish_saved = DisableGlobalIRQ();
    atomic_store_explicit(&state->busy, false, memory_order_release);
    pending_pins[(unsigned)pin] = false;
    /* An ISR may have queued work while this blocking read was in progress. */
    dispatch_next();
    EnableGlobalIRQ(finish_saved);
    return HAL_OK;
}

static hal_error_t start_conversion(pin_num_t pin,
                                    adc_isr_t handler,
                                    void *context)
{
    adc_channel_t adc;
    if (handler == NULL) return HAL_ERROR_INVALID_ARGUMENT;
    if (!adc_channel(pin, &adc)) return HAL_ERROR_NOT_SUPPORTED;
    const uint32_t saved = DisableGlobalIRQ();
    if (!initialized_pins[(unsigned)pin]) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_NOT_INITIALIZED;
    }
    if (pending_pins[(unsigned)pin] || request_count == ADC_REQUEST_CAPACITY) {
        EnableGlobalIRQ(saved);
        return HAL_ERROR_BUSY;
    }
    const unsigned tail = (request_head + request_count) % ADC_REQUEST_CAPACITY;
    requests[tail] = (adc_request_t){adc, handler, context};
    ++request_count;
    pending_pins[(unsigned)pin] = true;
    if (!atomic_exchange_explicit(&conversion_busy, true, memory_order_acquire))
        dispatch_next();
    EnableGlobalIRQ(saved);
    return HAL_OK;
}

static void adc_irq(unsigned instance)
{
    ADC_Type *const base = instance == 0u ? ADC0 : ADC1;
    adc_state_t *state = &states[instance];
    /* A stale NVIC interrupt must not consume a blocking read's FIFO result. */
    if (state->active_handler == NULL) return;
    lpadc_conv_result_t result;
    if (!LPADC_GetConvResult(base, &result, 0u)) return;

    LPADC_DisableInterrupts(base, kLPADC_FIFO0WatermarkInterruptEnable);
    const adc_isr_t handler = state->active_handler;
    const pin_num_t pin = state->active_pin;
    void *const context = state->active_context;
    const uint32_t saved = DisableGlobalIRQ();
    state->active_handler = NULL;
    state->active_context = NULL;
    atomic_store_explicit(&state->busy, false, memory_order_release);
    pending_pins[(unsigned)pin] = false;
    request_head = (request_head + 1u) % ADC_REQUEST_CAPACITY;
    --request_count;
    EnableGlobalIRQ(saved);
    /* Keep conversion_busy reserved across the callback. Reentrant submissions
     * append to the FIFO; the next hardware conversion starts only afterward. */
    if (handler != NULL) handler(pin, result.convValue, context);
    const uint32_t finish_saved = DisableGlobalIRQ();
    dispatch_next();
    EnableGlobalIRQ(finish_saved);
}

void ADC0_DriverIRQHandler(void) { adc_irq(0u); }
void ADC1_DriverIRQHandler(void) { adc_irq(1u); }

static const adc_driver_t driver = {
    .supports = supports,
    .init = init,
    .deinit = deinit,
    .read_raw = read_raw,
    .start_conversion = start_conversion,
};

const adc_driver_t *get_lpadc_driver(void) { return &driver; }
