/*
 * Copyright 2016-2026 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    02_gpio.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"

#define SW PIN_3_20
#define LED_GREEN PIN_0_27

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

    gpio_init_pin(SW, &(gpio_config_t){
    		.direction = GPIO_INPUT,
    		.pull = GPIO_PULL_UP,
    		.initial_level = 0
    });

    gpio_init_pin(SW, &(gpio_config_t){
    	.direction = GPIO_INPUT,
    	.pull = GPIO_PULL_NONE,
		.initial_level = 0,
    });

    printf("Button pressed r\n");

	int i = 0;
	bool last_press = 0;
	bool led = 1;

	while(1){
		bool pressed;
		gpio_read_pin(SW, &pressed);
		if(pressed == 0 && last_press == 1){
			printf("%3d Button pressed \r\n", i++);
			gpio_write_pin(LED_GREEN, led = !led);
		}
		last_press = pressed;
	}
    return 0 ;
}
