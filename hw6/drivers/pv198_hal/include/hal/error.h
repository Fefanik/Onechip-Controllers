#pragma once

typedef enum {
    HAL_OK                        = 0,
    HAL_ERROR_INVALID_ARGUMENT   = -1,
    HAL_ERROR_NOT_SUPPORTED      = -2,
    HAL_ERROR_NOT_INITIALIZED    = -3,
    HAL_ERROR_ALREADY_INITIALIZED = -4,
    HAL_ERROR_BUSY               = -5,
    HAL_ERROR_TIMEOUT            = -6,
    HAL_ERROR_IO                 = -7,
    HAL_ERROR_NACK               = -8,
} hal_error_t;
