#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    BELL_STATE_IDLE    = 0,
    BELL_STATE_RINGING = 1,
    BELL_STATE_PANIC   = 2
} BELL_STATE_E;

esp_err_t
RingBell_Init(void);

esp_err_t
RingBell_Run(void);

esp_err_t
RingBell_Stop(void);

/**
 * @brief Ring for a specified duration then auto-stop.
 */
esp_err_t
RingBell_RunForDuration(uint32_t ulDurationSec);

/**
 * @brief Enable/disable panic (non-stop) mode. Persisted in NVS.
 */
esp_err_t
RingBell_SetPanic(bool bEnable);

/**
 * @brief Get current bell state.
 */
BELL_STATE_E
RingBell_GetState(void);

/**
 * @brief Check if panic mode is active.
 */
bool
RingBell_IsPanic(void);