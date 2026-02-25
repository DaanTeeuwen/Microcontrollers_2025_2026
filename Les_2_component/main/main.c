#include <stdio.h>
#include "myGPIO.h"


#define LED 17
#define BUTTON 27

void app_main(void)
{
    myGPIO_LED_Setup(LED);
    myGPIO_LED_On(LED);
    myGPIO_BUTTON_Setup(BUTTON);
    
    while(1)
    {
        int counter = 0;
        
       
        while(counter < 5)
        {
            if(myGPIO_BUTTON_GetFlank(BUTTON))
            {
                counter++;
            }
        }
        
        
        myGPIO_LED_Toggle(LED);
    }
}
