#include <stdio.h>
#include "myADC.h"
#include "myGPIO.h"

#define ADC_PIN 0
#define BUTTON_PIN 4
#define LED_PIN 2

int code[4]; 
int temp_code[4]; 

void setup(void);
void code_in(void);
void dicht(void);
void code_in2(void);
void safe_open(void);

void setup(void)
{
    myADC_setup();
    myGPIO_KNOP_Setup(BUTTON_PIN);
    myGPIO_LED_Setup(LED_PIN);
    myGPIO_LED_Off(LED_PIN);  
}

void safe_open(void)
{
    myGPIO_LED_On(LED_PIN);
    printf("Safe is open\n");
}

void dicht(void)
{
    myGPIO_LED_Off(LED_PIN);
    printf("Safe is closed\n");
}

int get_digit(void)
{
    int adc_value = myADC_getValue();
    int digit = adc_value / 410;  
    if (digit > 9) digit = 9;
    return digit;
}

void wait_for_button(void)
{
    while (myGPIO_KNOP_GetValue(BUTTON_PIN) == 1);  
    while (myGPIO_KNOP_GetValue(BUTTON_PIN) == 0);  
}

/*typedef struct
 {

    int teller;
    int noemer;
}breuk_t;*/


void code_in(void)
{
    printf("Enter 4-digit code:\n");
    for (int i = 0; i < 4; i++)
    {
        printf("Enter digit %d: ", i+1);
        wait_for_button();
        code[i] = get_digit();
        printf("%d\n", code[i]);
    }
}

void code_in2(void)
{
    printf("Confirm 4-digit code:\n");
    for (int i = 0; i < 4; i++)
    {
        printf("Enter digit %d: ", i+1);
        wait_for_button();
        temp_code[i] = get_digit();
        printf("%d\n", temp_code[i]);
    }
    
    int match = 1;
    for (int i = 0; i < 4; i++)
    {
        if (temp_code[i] != code[i])
        {
            match = 0;
            break;
        }
    }
    if (match)
    {
        printf("Code confirmed, safe will close\n");
    }
    else
    {
        printf("Code does not match, try again\n");
        code_in2(); 
    }
}

void app_main(void)
{
    setup();
    safe_open();
    code_in();
    code_in2();
    dicht();

/*   breuk_t a;
    a.teller = */
    
}


