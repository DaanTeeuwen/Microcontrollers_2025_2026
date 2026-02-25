#include <stdio.h>
#include "myGPIO.h"
#include "driver/gpio.h"
#include "esp_attr.h"



void myGPIO_LED_Setup(int pin)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
}

void myGPIO_LED_On(int pin)
{
    gpio_set_level(pin, 1);
}

void myGPIO_LED_Off(int pin)
{
    gpio_set_level(pin, 0);
}

void myGPIO_LED_SetValue(int pin, int value)
{
    gpio_set_level(pin, value);
}

void myGPIO_LED_Toggle(int pin)
{
    /* int value = gpio_get_level(pin);
    gpio_set_level(pin, !value);
    return value; */

    gpio_set_level(pin, !gpio_get_level(pin));
}

void myGPIO_BUTTON_Setup(int button)
{
    gpio_reset_pin(button);
    gpio_set_direction(button, GPIO_MODE_INPUT);
    gpio_set_pull_mode(button, GPIO_PULLUP_ONLY);
}

int myGPIO_BUTTON_GetValue(int button)
{
    return gpio_get_level(button);
}

int myGPIO_BUTTON_GetFlank(int button)
{
    static int before, now = 1;
   
        before = now;
        now = gpio_get_level(button);

        if (before != now)
        {
          return 1;
        }
        return 0;
}