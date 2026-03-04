#include <stdio.h>
#include "myPWM.h"
#include "driver/ledc.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL_T LEDC_CHANNEL_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_OUTPUT_IO (5)
#define LEDC_DUTY (4096)
#define LEDC_FREQUENCY (4000)

void myPWM_setup(void)
{
    ledc_timer_config_t ledc_timer =
    {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_T,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void myPWM_set_duty(int value)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_T, value);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_T);
}
