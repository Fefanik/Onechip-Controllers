#include "internal.h"

#include "fsl_lpuart.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    STDIO_FLEXCOMM = 4u,
    STDIO_BAUD_RATE = 115200u,
};

static bool stdio_initialized;

void hal_stdio_init(void)
{
    if (hal_flexcomm_claim(STDIO_FLEXCOMM, HAL_PIN_OWNER_UART) != HAL_OK)
        return;
    if (hal_pin_claim(PIN_1_9, HAL_PIN_OWNER_UART) != HAL_OK) {
        hal_flexcomm_release(STDIO_FLEXCOMM, HAL_PIN_OWNER_UART);
        return;
    }
    if (hal_pin_claim(PIN_1_8, HAL_PIN_OWNER_UART) != HAL_OK) {
        hal_pin_release(PIN_1_9, HAL_PIN_OWNER_UART);
        hal_flexcomm_release(STDIO_FLEXCOMM, HAL_PIN_OWNER_UART);
        return;
    }

    hal_pin_configure(PIN_1_9, kPORT_MuxAlt2, GPIO_PULL_NONE, false, true);
    hal_pin_configure(PIN_1_8, kPORT_MuxAlt2, GPIO_PULL_NONE, false, true);

    const uint32_t clock_hz = hal_flexcomm_enable_clock(STDIO_FLEXCOMM);
    lpuart_config_t config;
    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = STDIO_BAUD_RATE;
    config.enableTx = true;
    config.enableRx = true;
    if (LPUART_Init(LPUART4, &config, clock_hz) == kStatus_Success) {
        stdio_initialized = true;
        return;
    }

    hal_pin_make_safe(PIN_1_8);
    hal_pin_make_safe(PIN_1_9);
    hal_pin_release(PIN_1_8, HAL_PIN_OWNER_UART);
    hal_pin_release(PIN_1_9, HAL_PIN_OWNER_UART);
    hal_flexcomm_release(STDIO_FLEXCOMM, HAL_PIN_OWNER_UART);
}

int _write(int handle, char *buffer, int size)
{
    if ((handle != 1 && handle != 2) || buffer == NULL || size < 0) {
        errno = EINVAL;
        return -1;
    }
    if (!stdio_initialized ||
        LPUART_WriteBlocking(LPUART4, (const uint8_t *)buffer, (size_t)size) != kStatus_Success) {
        errno = EIO;
        return -1;
    }
    return size;
}

int _read(int handle, char *buffer, int size)
{
    if (handle != 0 || buffer == NULL || size < 0) {
        errno = EINVAL;
        return -1;
    }
    if (!stdio_initialized ||
        LPUART_ReadBlocking(LPUART4, (uint8_t *)buffer, (size_t)size) != kStatus_Success) {
        errno = EIO;
        return -1;
    }
    return size;
}
