#include <stdbool.h>
#include "RingBell_API.h"
#include "driver/gpio.h"

#define RING_BELL_START_STOP_PIN           GPIO_NUM_4  /* D4 */
#define RING_BELL_STATE_ON                 ((int)1)
#define RING_BELL_STATE_OFF                ((int)0)

esp_err_t
ringBell_SetState(int iState);

esp_err_t
RingBell_Run(void)
{
    return ringBell_SetState(RING_BELL_STATE_ON);
}

esp_err_t
RingBell_Stop(void)
{
    return ringBell_SetState(RING_BELL_STATE_OFF);
}

esp_err_t
ringBell_SetState(int iState)
{
    esp_err_t espErr = ESP_OK;
    gpio_config_t tGpio2 = 
    {
        .pin_bit_mask = (1ULL << RING_BELL_START_STOP_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    espErr = gpio_config(&tGpio2);

    if(ESP_OK == espErr)
    {
        switch(iState)
        {
            case RING_BELL_STATE_ON:
            case RING_BELL_STATE_OFF:
            {
                espErr = gpio_set_level(RING_BELL_START_STOP_PIN, 
                                        iState);
                break;
            }

            default:
            {
                espErr = ESP_ERR_INVALID_STATE;
                break;
            }
        } /* switch */
    } /* if(ESP_OK == espErr) */

    return espErr;
}