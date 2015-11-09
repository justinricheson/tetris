#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "drivers/rit128x96x4.h"
#include <stdlib.h>
#include "audio.h"
#include "globals.h"
#include "sounds.h"
#include "graphics.h"

// Called on driver library error
#ifdef DEBUG
void __error__(char *pcFilename, unsigned long ulLine)
{
}
#endif

unsigned long g_ulSystemClock;
const int timerDivisor = 100;

// Control flags
int tick = 0;
int first = 1;

// Game State

// Button States
int b0 = 0; // D
int b1 = 0; // U
int b2 = 0; // L
int b3 = 0; // R
int b4 = 0; // RR

inline int ButtonUp(int curValue, int preValue)
{
    if(!curValue && preValue)
    {
        return 1;
    }

    return 0;
}

inline int ButtonDown(int curValue, int preValue)
{
    if(curValue && !preValue)
    {
        return 1;
    }

    return 0;
}

inline int ButtonChanged(int curValue, int preValue)
{
    return curValue != preValue;
}

inline int ValidButtonCombo(int b0, int b1, int b2, int b3, int b4)
{
    if(b0)
    {
        return 0;
    }

    int numPressed = b1 + b2 + b3 + b4;

    if(b4)
    {
        return numPressed <= 2; // Select can be pressed alone of with any direction button
    }

    // Otherwise, at most one button can be pressed at a time
    return numPressed <= 1;
}

void Timer0IntHandler(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    AudioHandler(); // Play sounds

    // Get button states
    unsigned long buttons;
    buttons = (GPIOPinRead(GPIO_PORTE_BASE, (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)) |
              (GPIOPinRead(GPIO_PORTF_BASE,  GPIO_PIN_1) << 3));

    int b0_t = !(buttons & 1);
    int b1_t = !((buttons & 2) >> 1);
    int b2_t = !((buttons & 4) >> 2);
    int b3_t = !((buttons & 8) >> 3);
    int b4_t = !((buttons & 16) >> 4);
    if(ValidButtonCombo(b0_t, b1_t, b2_t, b3_t, b4_t))
    {
        // TODO update game state
    }

    if(ButtonChanged(b0_t, b0)
    || ButtonChanged(b1_t, b1)
    || ButtonChanged(b2_t, b2)
    || ButtonChanged(b3_t, b3)
    || ButtonChanged(b4_t, b4))
    {
        tick = 1;
    }

    // Store button states
    b0 = b0_t;
    b1 = b1_t;
    b2 = b2_t;
    b3 = b3_t;
    b4 = b4_t;

    // Trigger the event the first time so the initial screen is drawn
    if(first)
    {
        first = 0;
        tick = 1; // Signal event
    }
}

char * intToString(int input)
{
    // Convert int to char pointer
    // Only works with non-negative values 0 - 99
    int flag = 0;
    int i = 1;
    char str[3] = { '0', '0', '\0' };
    while(input != 0)
    {
        str[i--] = (input % 10) + '0';
        input /= 10;
        flag = 1;
    }

    if(!flag) // Input == 0
    {
        return "00";
    }

    char *result = malloc(3);
    if(result == NULL)
    {
        return NULL;
    }

    { // Additional scope so we don't need to define j up top, CC Studio is C89
        int j;
        for(j = 0; j < 3; j++)
        {
            result[j] = str[j];
        }
    }

    return result;
}

int main(void)
{
    // Init clocks
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
    SysCtlPWMClockSet(SYSCTL_PWMDIV_8);

    // Init screen
    RIT128x96x4Init(1000000);

    // Get system clock
    g_ulSystemClock = SysCtlClockGet();

    // Enable peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);

    // Init timer
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet() / timerDivisor);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER0_BASE, TIMER_A);

    // Init buttons
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

    // Init sound
    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
    AudioOn();

    //char *pTimeHours = intToString(hours);
    //RIT128x96x4StringDraw(pTimeHours, 0, 0, 15);
    //AudioPlaySound(g_pusFireEffect, sizeof(g_pusFireEffect) / 2);

    // Draw test grid TODO: remove
    unsigned char *bmp = (unsigned char *)&block[BITMAP_HEADER_SIZE];
    unsigned long height = (unsigned long)block[BITMAP_HEIGHT_OFFSET];
    unsigned long width = (unsigned long)block[BITMAP_WIDTH_OFFSET];

    int i, j; // C89 sucks
    for(i = 0; i < 20; i++)
    {
    	int y = 16 + (height * i);
        for(j = 0; j < 10; j++)
        {
        	int x = 44 + (width * j);
            RIT128x96x4ImageDraw(bmp, x, y, width, height);
        }
    }

    IntMasterEnable();
    while(1)
    {
        while(!tick); // Wait for events

        IntMasterDisable();
        tick = 0; // Reset event flag

        RIT128x96x4StringDraw(b0 ? "ON " : "OFF", 00, 0, 15);
        RIT128x96x4StringDraw(b1 ? "ON " : "OFF", 20, 0, 15);
        RIT128x96x4StringDraw(b2 ? "ON " : "OFF", 40, 0, 15);
        RIT128x96x4StringDraw(b3 ? "ON " : "OFF", 60, 0, 15);
        RIT128x96x4StringDraw(b4 ? "ON " : "OFF", 80, 0, 15);

        IntMasterEnable();
    }
}
