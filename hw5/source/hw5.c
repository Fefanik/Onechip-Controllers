#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "hal.h"

#define POTENTIOMETER_PIN  PIN_4_2
#define BLINK_TIMER        TIMER1
#define BUTTON_TIMER       TIMER0
#define TICK_HZ            1000000u
#define DEBOUNCE_MS        30u
#define LONG_PRESS_MS      500u
#define MIN_FREQUENCY_HZ   0.2f
#define MAX_FREQUENCY_HZ   2.0f
#define LED_COUNT          3u

typedef enum {
    LED_RED,
    LED_GREEN,
    LED_BLUE
} led_t;

typedef enum {
    PRESS_NONE,
    PRESS_SHORT,
    PRESS_LONG
} press_t;

typedef struct {
    volatile bool lock;
    volatile uint32_t press_started;
    volatile press_t press;
} button_t;

const pin_num_t led_pins[LED_COUNT] = {
    PIN_LED_RED,
    PIN_LED_GREEN,
    PIN_LED_BLUE
};

volatile led_t selected_led = LED_RED;
volatile bool potentiometer_sample_ready = false;
volatile uint16_t potentiometer_sample = 0;

uint32_t milliseconds_to_ticks(uint32_t milliseconds)
{
    return TICK_HZ / 1000u * milliseconds;
}

void initialize_leds(void)
{
    const gpio_config_t config = {GPIO_OUTPUT, GPIO_PULL_NONE, true};

    for (uint32_t i = 0; i < LED_COUNT; ++i) {
        gpio_init_pin(led_pins[i], &config);
    }
}

void select_led(led_t led)
{
    for (uint32_t i = 0; i < LED_COUNT; ++i) {
        if (i == led) {
            gpio_write_pin(led_pins[i], 0);
        } else {
            gpio_write_pin(led_pins[i], 1);
        }
    }
}

void blink_timer_handler(timer_num_t timer, void *context)
{
    gpio_toggle_pin(led_pins[selected_led]);
}

uint32_t sample_to_toggle_interval_ticks(uint16_t sample)
{
    const float ratio = (float)sample / (float)UINT16_MAX;
    const float frequency = MIN_FREQUENCY_HZ
            + ratio * (MAX_FREQUENCY_HZ - MIN_FREQUENCY_HZ);
    return (uint32_t)((float)TICK_HZ / (2.0f * frequency) + 0.5f);
}

void potentiometer_conversion_handler(pin_num_t pin, uint16_t sample, void *context)
{
    potentiometer_sample = sample;
    potentiometer_sample_ready = true;
}

void start_blink_timer(uint32_t toggle_interval_ticks)
{
    timer_init(BLINK_TIMER, TICK_HZ);
    timer_match_start_periodic(BLINK_TIMER, TIMER_MATCH0, toggle_interval_ticks,
                               blink_timer_handler, NULL);
}

void update_blink_timer(void)
{
    timer_stop(BLINK_TIMER);
    timer_deinit(BLINK_TIMER);
    start_blink_timer(sample_to_toggle_interval_ticks(potentiometer_sample));
}

void button_capture_handler(pin_num_t pin, gpio_interrupt_t edge,
                            uint32_t ticks, void *context)
{
    button_t *button = context;

    if (edge == GPIO_FALLING_EDGE) {
        if (!button->lock) {
            button->press_started = ticks;
            button->lock = true;
        }
    } else if (button->lock) {
        const uint32_t duration = ticks - button->press_started;
        if (duration < milliseconds_to_ticks(DEBOUNCE_MS)) {
            button->press = PRESS_NONE;
        } else if (duration < milliseconds_to_ticks(LONG_PRESS_MS)) {
            button->press = PRESS_SHORT;
        } else {
            button->press = PRESS_LONG;
        }
        button->lock = false;
    }
}

void process_button_press(button_t *button)
{
    const press_t press = button->press;
    button->press = PRESS_NONE;

    if (press == PRESS_LONG) {
        select_led((led_t)(((uint32_t)selected_led + 1u) % LED_COUNT));
    } else if (press == PRESS_SHORT) {
        adc_start_conversion(POTENTIOMETER_PIN);
    }
}

void update_from_potentiometer_sample(void)
{
    if (potentiometer_sample_ready) {
        potentiometer_sample_ready = false;
        update_blink_timer();
    }
}

int main(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    initialize_leds();

    button_t button = {0};
    gpio_init_pin(PIN_BUTTON_SW2, &(gpio_config_t){GPIO_INPUT, GPIO_PULL_UP, true});
    timer_init(BUTTON_TIMER, TICK_HZ);
    timer_capture_init(BUTTON_TIMER, PIN_BUTTON_SW2, GPIO_ANY_EDGE,
                       button_capture_handler, &button);
    timer_start(BUTTON_TIMER);

    adc_init(POTENTIOMETER_PIN);
    adc_register_interrupt(POTENTIOMETER_PIN, potentiometer_conversion_handler, NULL);

    select_led(LED_RED);
    start_blink_timer(sample_to_toggle_interval_ticks(UINT16_MAX / 2u));

    while (1) {
        if (button.press != PRESS_NONE) {
            process_button_press(&button);
        }
        update_from_potentiometer_sample();
        delay_ms(1);
    }
}
