// __LED's__

void myGPIO_LED_Setup(int pin);
void myGPIO_LED_On(int pin);
void myGPIO_LED_Off(int pin);
void myGPIO_LED_SetValue(int pin, int value);
void myGPIO_LED_Toggle(int pin);


// __KNOPPEN__

void myGPIO_BUTTON_Setup(int pin);
int myGPIO_BUTTON_GetValue(int pin);
int myGPIO_BUTTON_GetFlank(int pin); 