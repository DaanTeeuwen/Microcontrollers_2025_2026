#include <stdio.h>
#include "myADC.h"
#include "myGPIO.h"

#define RED 2 
#define GREEN 3
#define BLUE 4

void app_main(void)
{
    myADC_setup();
    myGPIO_LED_Setup(RED);
    myGPIO_LED_Setup(GREEN);
    myGPIO_LED_Setup(BLUE);

int value;

    while (1)
    {
        value = myADC_getValue();
        printf("De waarde is: %d \n" , value );

        if (value < 300)
        {
            myGPIO_LED_On(RED);
            myGPIO_LED_Off(GREEN);
            myGPIO_LED_Off(BLUE);
        }
        else if (value > 300 && value < 600)
        {
            myGPIO_LED_On(GREEN);
            myGPIO_LED_Off(RED);
            myGPIO_LED_Off(BLUE);
        }
        else
        {
            myGPIO_LED_On(BLUE);
            myGPIO_LED_Off(RED);
            myGPIO_LED_Off(GREEN);
        }
    }
}


