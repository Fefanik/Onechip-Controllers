#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "hal.h"

#define JOYSTICK_SERVO_ADC_PIN PIN_ANA_0
#define JOYSTICK_LED_ADC_PIN   PIN_ANA_1
#define JOYSTICK_BUTTON_PIN    PIN_0_29

#define SERVO_TIMER            TIMER1
#define LED_TIMER              TIMER0
#define TIMER_TICK_HZ          1000000u
#define SERVO_PERIOD_TICKS     20000u
#define LED_PERIOD_TICKS       1000u
#define SERVO_MIN_PULSE_TICKS  500u
#define SERVO_MAX_PULSE_TICKS  2500u

typedef enum {
    ADC_AXIS_SERVO,
    ADC_AXIS_LED
} adc_axis_t;

typedef struct {
    volatile bool locked;
    volatile adc_axis_t next_axis;
    volatile uint32_t servo_pulse;
    volatile uint32_t led_pulse;
} joystick_context_t;

joystick_context_t joystick = {
    .locked = false,
    .next_axis = ADC_AXIS_SERVO,
    .servo_pulse = (SERVO_MIN_PULSE_TICKS + SERVO_MAX_PULSE_TICKS) / 2u,
    .led_pulse = LED_PERIOD_TICKS / 2u
};

void err_check(hal_error_t status, const char *operation)
{
    if (status != HAL_OK) {
        printf("HAL error: %d --> %s\r\n", status, operation);
    }
}

uint32_t scale_sample(uint16_t sample, uint32_t output_max)
{
    return (uint32_t)(((uint64_t)sample * output_max + UINT16_MAX / 2u)
                      / UINT16_MAX);
}

uint32_t servo_pulse_ticks(uint16_t sample)
{
    return SERVO_MIN_PULSE_TICKS
            + scale_sample(sample, SERVO_MAX_PULSE_TICKS
                                  - SERVO_MIN_PULSE_TICKS);
}

uint32_t led_pulse_ticks(uint16_t sample)
{
    const uint32_t brightness_ticks = scale_sample(sample, LED_PERIOD_TICKS);
    return LED_PERIOD_TICKS - brightness_ticks;
}

void joystick_adc_handler(pin_num_t pin, uint16_t sample, void *context)
{
    joystick_context_t *state = context;

    if (state->locked) {
        return;
    }

    if (pin == JOYSTICK_SERVO_ADC_PIN) {
        state->servo_pulse = servo_pulse_ticks(sample);
        state->next_axis = ADC_AXIS_LED;
    } else if (pin == JOYSTICK_LED_ADC_PIN) {
        state->led_pulse = led_pulse_ticks(sample);
        state->next_axis = ADC_AXIS_SERVO;
    } else {
        return;
    }

    if (!state->locked) {
        const pin_num_t next_pin = state->next_axis == ADC_AXIS_SERVO
                ? JOYSTICK_SERVO_ADC_PIN : JOYSTICK_LED_ADC_PIN;
        err_check(adc_start_conversion(next_pin), "NEXT ADC CONVERSION");
    }
}

void pwm_period_handler(timer_num_t timer, void *context)
{
    joystick_context_t *state = context;

    if (timer == SERVO_TIMER) {
        err_check(pwm_set_pulse_width(SERVO_TIMER, PIN_1_3,
                                      state->servo_pulse),
                  "SERVO PWM UPDATE");
    } else if (timer == LED_TIMER) {
        err_check(pwm_set_pulse_width(LED_TIMER, PIN_LED_RED,
                                      state->led_pulse),
                  "LED PWM UPDATE");
    }
}

void joystick_button_handler(gpio_port_t port, uint32_t pending_mask,
                             void *context)
{
    joystick_context_t *state = context;
    const uint32_t button_mask = 1u << PIN_INDEX(JOYSTICK_BUTTON_PIN);

    if (port != PIN_PORT(JOYSTICK_BUTTON_PIN)
            || (pending_mask & button_mask) == 0u) {
        return;
    }

    bool pin_high;
    if (gpio_read_pin(JOYSTICK_BUTTON_PIN, &pin_high) != HAL_OK || pin_high) {
        return;
    }

    state->locked = !state->locked;

    if (!state->locked) {
        state->next_axis = ADC_AXIS_SERVO;
        err_check(adc_start_conversion(JOYSTICK_SERVO_ADC_PIN),
                  "SERVO ADC CONVERSION");
    }
}

void initialize_servo_pwm(void)
{
    err_check(timer_init_period(SERVO_TIMER, TIMER_TICK_HZ, SERVO_PERIOD_TICKS),
              "SERVO TIMER INIT");
    err_check(pwm_init(SERVO_TIMER, PIN_1_3,
                       SERVO_MIN_PULSE_TICKS
                               + (SERVO_MAX_PULSE_TICKS - SERVO_MIN_PULSE_TICKS) / 2u),
              "SERVO PWM INIT");
    err_check(timer_match_set_interrupt(SERVO_TIMER, TIMER_MATCH2,
                                        pwm_period_handler, &joystick),
              "SERVO PERIOD INTERRUPT");
    err_check(timer_start(SERVO_TIMER), "SERVO TIMER START");
}

void initialize_led_pwm(void)
{
    err_check(timer_init_period(LED_TIMER, TIMER_TICK_HZ, LED_PERIOD_TICKS),
              "LED TIMER INIT");
    err_check(pwm_init(LED_TIMER, PIN_LED_RED, LED_PERIOD_TICKS / 2u),
              "LED PWM INIT");
    err_check(timer_match_set_interrupt(LED_TIMER, TIMER_MATCH2,
                                        pwm_period_handler, &joystick),
              "LED PERIOD INTERRUPT");
    err_check(timer_start(LED_TIMER), "LED TIMER START");
}

void initialize_joystick_adcs(void)
{
    err_check(adc_init(JOYSTICK_SERVO_ADC_PIN), "SERVO ADC INIT");
    err_check(adc_init(JOYSTICK_LED_ADC_PIN), "LED ADC INIT");
    err_check(adc_register_interrupt(JOYSTICK_SERVO_ADC_PIN,
                                     joystick_adc_handler, &joystick),
              "SERVO ADC INTERRUPT");
    err_check(adc_register_interrupt(JOYSTICK_LED_ADC_PIN,
                                     joystick_adc_handler, &joystick),
              "LED ADC INTERRUPT");
}

void initialize_joystick_button(void)
{
    err_check(gpio_init_pin(JOYSTICK_BUTTON_PIN,
                            &(gpio_config_t){GPIO_INPUT, GPIO_PULL_UP, true}),
              "JOYSTICK BUTTON INIT");
    err_check(gpio_register_port_interrupt(
                      PIN_PORT(JOYSTICK_BUTTON_PIN),
                      1u << PIN_INDEX(JOYSTICK_BUTTON_PIN), GPIO_ANY_EDGE,
                      joystick_button_handler, &joystick),
              "JOYSTICK BUTTON INTERRUPT");
}

void initialize_joystick_controls(void)
{
    initialize_servo_pwm();
    initialize_led_pwm();
    initialize_joystick_adcs();
    initialize_joystick_button();

    err_check(adc_start_conversion(JOYSTICK_SERVO_ADC_PIN),
              "INITIAL SERVO ADC CONVERSION");
}

void print_lock_state_changes(void)
{
    static bool reported_lock_state = false;

    if (joystick.locked != reported_lock_state) {
        reported_lock_state = joystick.locked;
        printf("Joystick lock: %s\r\n",
               reported_lock_state ? "locked" : "unlocked");
    }
}

int main(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    initialize_joystick_controls();

    while (1) {
        print_lock_state_changes();
    }
}
