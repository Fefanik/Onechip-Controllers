/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    sm3.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
/* TODO: insert other include files here. */

/* TODO: insert other definitions and declarations here. */

/*
 * @brief   Application entry point.
 */
int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    gpio_init_pin(PIN_BUTTON_SW3, &(gpio_config_t){
        .direction = GPIO_INPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 0,
    });

    gpio_init_pin(PIN_LED_RED, &(gpio_config_t){
        .direction = GPIO_OUTPUT,
        .pull = GPIO_PULL_NONE,
        .initial_level = 1
    });


    printf("Hello World\r\n");
    adc_init(PIN_4_2);
    /* Force the counter to be placed into memory. */
    volatile static int i = 0 ;
    /* Enter an infinite loop, just incrementing a counter. */
    int old_status_3 = 1;

    int threshold = 10000000000000;
    while(1) {
        i++ ;
        /* 'Dummy' NOP to allow source level single stepping of
            tight while() loop */

        bool status3;
        gpio_read_pin(PIN_BUTTON_SW3, &status3);

        if (status3 != old_status_3) {
        	uint16_t sample;
        	threshold = adc_read_raw(PIN_4_2,&sample);
        	old_status_3 = status3;
        	}

        uint16_t millivolts;
        adc_read_raw(PIN_4_2, &millivolts);
        printf("%d",millivolts);
        gpio_write_pin(PIN_LED_RED, (millivolts > threshold) ? 0 : 1);



        __asm volatile ("nop");
    }
    return 0 ;
}
