/*
 * Copyright 2016-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    sem02.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"

#define SW2 PIN_BUTTON_SW2
#define SW3 PIN_BUTTON_SW3

void init_hardware(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    gpio_init_pin(SW2, &(gpio_config_t){
        .direction = GPIO_INPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 0,
    });

    gpio_init_pin(SW3, &(gpio_config_t){
        .direction = GPIO_INPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 0,
    });

    gpio_init_pin(PIN_LED_RED, &(gpio_config_t){
        .direction = GPIO_OUTPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 1
    });
    gpio_init_pin(PIN_LED_GREEN, &(gpio_config_t){
        .direction = GPIO_OUTPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 1
    });
    gpio_init_pin(PIN_LED_BLUE, &(gpio_config_t){
        .direction = GPIO_OUTPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 1
    });
}

void handle_sw3(int color, int *old_status_3, bool *red_on, bool *green_on, bool *blue_on) {
    bool status3;
    gpio_read_pin(SW3, &status3);

    if (status3 == 0 && status3 != *old_status_3) {
        delay_ms(20);
        gpio_read_pin(SW3, &status3);
        if (status3 == 0) {
            if (color == 0) {
                *red_on = !(*red_on);
                gpio_write_pin(PIN_LED_RED, (*red_on) ? 0 : 1);
            } else if (color == 1) {
                *green_on = !(*green_on);
                gpio_write_pin(PIN_LED_GREEN, (*green_on) ? 0 : 1);
            } else {
                *blue_on = !(*blue_on);

                gpio_write_pin(PIN_LED_BLUE, (*blue_on) ? 0 : 1);
            }
        }
    }
    *old_status_3 = status3;
}

void handle_sw2(int *color, int *old_status_2) {
    bool status2;
    gpio_read_pin(SW2, &status2);

    if (status2 == 0 && status2 != *old_status_2) {
        delay_ms(20);
        gpio_read_pin(SW2, &status2);
        if (status2 == 0) {
            (*color)++;
            *color %= 3;
            printf("Color set: %d\r\n", *color);
        }
    }
    *old_status_2 = status2;
}



int main(void) {


	init_hardware();

    volatile static int i = 0 ;

    int old_status2 = 1;
    int old_status3 = 1;
    int color = 0;

    bool red_on = false;
    bool green_on = false;
    bool blue_on = false;



    while(1) {
        i++ ;
        handle_sw2(&color, &old_status2);
        handle_sw3(color, &old_status3, &red_on, &green_on, &blue_on);
        __asm volatile ("nop");
    }
    return 0;
}
