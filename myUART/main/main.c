#include <stdio.h>
#include "myUART.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "esp_random.h"

void random_getal(void);



int wacht_op_getal(void)
{
    int getal = 0;
     while (getal  == 0)
     {
        scanf("%d", &getal); 
        vTaskDelay(1);
     }
     return getal;

}


void app_main(void)
{
     myUART_setup(115200);
     myUART_printf("Hallo\n");
     random_getal();
}

void random_getal(void)
{
    int randomGetal = esp_random() % 11;

    char ingevoerdGetal;

     myUART_printf("Voer een getal in: ");

     ingevoerdGetal = wacht_op_getal();
     
     myUART_printf("Je tikte %d\n", ingevoerdGetal);

     int i = 0;

    while (ingevoerdGetal != randomGetal)
     {
        i++;

        myUART_printf("Fout getal, aantal pogingen: %d\n", i);

        myUART_printf("Voer een nieuw getal in: ");
         
         ingevoerdGetal = wacht_op_getal();
     
        myUART_printf("Je tikte %d\n", ingevoerdGetal);

     }
     if (ingevoerdGetal == randomGetal)
     {
        myUART_printf("Goed geraden, het getal was: %d\n", randomGetal);
        myUART_printf("Aantal pogingen om het getal te raden: %d\n", i);
     }
     else
     {
        myUART_printf("Voer een geldig getal in");
     }

}