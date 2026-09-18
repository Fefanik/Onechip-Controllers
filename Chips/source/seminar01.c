#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"

void setup_led(){
	const gpio_config_t conf = {GPIO_OUTPUT, GPIO_PULL_NONE, false};
	gpio_init_pin(PIN_0_27, &conf);
}

void toggle_led() {
	gpio_toggle_pin(PIN_0_27);
}

int main(void) {
    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    printf("Hello World\r\n");

    setup_led();

    while(1) {
    	toggle_led();
    	delay_ms(500/2);
    }

    return 0 ;
}
