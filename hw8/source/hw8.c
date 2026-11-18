#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "hal.h"


#define UART_PORT UART_PORT_5
#define SERVO_TIMER TIMER1
#define SERVO_PIN PIN_1_3
#define SERVO_TICK_HZ 1000000u
#define SERVO_PERIOD_TICKS 20000u
#define SERVO_MIN_PULSE_TICKS 500u
#define SERVO_MAX_PULSE_TICKS 2500u

#define COMMAND_BUFFER_SIZE 16u

static void err_check(hal_error_t status, const char *what)
{
    if (status != HAL_OK) {
        printf("HAL error: %d --> %s\r\n", (int)status, what);
        while (1) {
        }
    }
}

static void uart_write_text(const char *text)
{
    size_t length = 0u;
    while (text[length] != '\0') {
        ++length;
    }
    err_check(uart_write(UART_PORT, (const uint8_t *)text, length), "UART WRITE");
}

static uint32_t servo_pulse_ticks(unsigned angle)
{
    return SERVO_MIN_PULSE_TICKS
        + (SERVO_MAX_PULSE_TICKS - SERVO_MIN_PULSE_TICKS) * angle / 180u;
}


static bool parse_command(const char *command, size_t length,
                          unsigned current_angle, unsigned *new_angle)
{
    if (length < 2u || length > 4u) {
        return false;
    }

    const char operation = command[0];
    if (operation != 'A' && operation != '+' && operation != '-') {
        return false;
    }

    unsigned value = 0u;
    for (size_t i = 1u; i < length; ++i) {
        if (command[i] < '0' || command[i] > '9') {
            return false;
        }
        value = value * 10u + (unsigned)(command[i] - '0');
    }

    if (operation == 'A') {
        *new_angle = value;
    } else if (operation == '+') {
        *new_angle = current_angle + value;
    } else {
        if (value > current_angle) {
            return false;
        }
        *new_angle = current_angle - value;
    }

    return *new_angle <= 180u;
}

static void send_angle_response(unsigned angle)
{
    char response[16];
    const int length = snprintf(response, sizeof(response), "\r\nOK %u\r\n", angle);
    if (length > 0) {
        err_check(uart_write(UART_PORT, (const uint8_t *)response, (size_t)length),
                  "UART WRITE");
    }
}

static void process_command(const char *command, size_t length, bool overflow,
                            unsigned *angle)
{
    unsigned new_angle;
    if (overflow || !parse_command(command, length, *angle, &new_angle)) {
        uart_write_text("\r\nERR\r\n");
        return;
    }

    err_check(pwm_set_pulse_width(SERVO_TIMER, SERVO_PIN,
                                  servo_pulse_ticks(new_angle)), "SERVO PWM");
    *angle = new_angle;
    send_angle_response(*angle);
}

int main(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    err_check(uart_init(UART_PORT, &(uart_config_t){
        .baud_rate = 115200u,
        .parity = UART_PARITY_NONE,
        .stop_bits = UART_STOP_BITS_1,
    }), "UART INIT");

    err_check(timer_init_period(SERVO_TIMER, SERVO_TICK_HZ, SERVO_PERIOD_TICKS),
              "SERVO TIMER INIT");
    err_check(pwm_init(SERVO_TIMER, SERVO_PIN, servo_pulse_ticks(90u)),
              "SERVO PWM INIT");
    err_check(timer_start(SERVO_TIMER), "SERVO TIMER START");

    unsigned angle = 90u;
    char command[COMMAND_BUFFER_SIZE];
    size_t length = 0u;
    bool overflow = false;

    while (1) {
        uint8_t character;
        size_t received;
        if (uart_read(UART_PORT, &character, 1u, &received) != HAL_OK) {
            length = 0u;
            overflow = false;
            uart_write_text("\r\nERR\r\n");
            continue;
        }
        if (received != 1u) {
            continue;
        }


        err_check(uart_write(UART_PORT, &character, 1u), "UART ECHO");

        if (character == '\r' || character == '\n') {
            if (length != 0u || overflow) {
                process_command(command, length, overflow, &angle);
            }
            length = 0u;
            overflow = false;
            continue;
        }

        if (!overflow) {
            if (length < sizeof(command)) {
                command[length++] = (char)character;
            } else {
                overflow = true;
            }
        }
    }
}
