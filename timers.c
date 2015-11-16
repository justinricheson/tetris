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

// Shapes
#define S_O 0
#define S_I 1
#define S_S 2
#define S_Z 3
#define S_L 4
#define S_J 5
#define S_T 6

// Orientations
#define O_000 0
#define O_090 1
#define O_180 2
#define O_270 3

// Timer stuff
unsigned long g_ulSystemClock;
const int timerDivisor = 100;

// Control flags
int tick = 0;
int first = 1;

// Game state
int shape = -1;
int orientation = -1;
int locationX = -1;
int locationY = -1;
int grid[200] = { 0 };

// Button states
int b0 = 0; // D
int b1 = 0; // U
int b2 = 0; // L
int b3 = 0; // R
int b4 = 0; // RR

// Graphics constants
const int height = 4;
const int width = 4;
const int xOffset = 44;
const int yOffset = 16;

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

    //AudioHandler(); // Play sounds

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
        if(ButtonUp(b4_t, b4))
        {
            if(orientation == O_000)
            {
                orientation = O_090;
            }
            else if(orientation == O_090)
            {
                orientation = O_180;
            }
            else if(orientation == O_180)
            {
                orientation = O_270;
            }
            else
            {
                orientation = O_000;
            }

            tick = 1;
        }

        int down = 0;
        if(ButtonDown(b1_t, b1))
        {
            shape = S_T; down = 1;
        }

        if(down)
        {
            orientation = O_000;
            locationX = 5;
            locationY = 10;
            tick = 1;
        }
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

void Transpose(int *a, int *b, int *c, int *d)
{
   int temp = *a;
   *a = *b;
   *b = *c;
   *c = *d;
   *d = temp;
}

void Rotate(int *m, int n)
{
    int i, j;
    for(i = 0; i < n / 2; i++)
    {
       for(j = 0; j < (n + 1) / 2; j++)
       {
           Transpose(&(m[i*n+j]), &(m[(n-1-j)*n+i]), &(m[(n-1-i)*n+(n-1-j)]), &(m[j*n+(n-1-i)]));
       }
    }
}

void RotateShape(int *m, int n)
{
    int rotations =
        shape == S_O ? 0 :
        (shape == S_I || shape == S_S || shape == S_Z) && (orientation == O_000 || orientation == O_180) ? 0 :
        (shape == S_I || shape == S_S || shape == S_Z) ? 1 :
        orientation == O_000 ? 0 :
        orientation == O_090 ? 1 :
        orientation == O_180 ? 2 : 3;

    int i;
    for(i = 0; i < rotations; i++)
    {
        Rotate(m, n);
    }
}

void DrawShape(int *m, int rows, int cols, int x, int y, unsigned char *bufferT, unsigned char *bufferF)
{
    int startX = xOffset + (width * x);
    int startY = yOffset + (height * y);

    int i, j;
    for(i = 0; i < rows; i++)
    {
        int yAbs = startY + (height * i);
        for(j = 0; j < cols; j++)
        {
            int xAbs = startX + (width * j);

            if(m[i * cols + j])
            {
                RIT128x96x4ImageDraw(bufferT, xAbs, yAbs, width, height);
            }
            else
            {
                RIT128x96x4ImageDraw(bufferF, xAbs, yAbs, width, height);
            }
        }
    }
}

int * Copy(const int *src, int len)
{
    int *copy = malloc(len * sizeof(int));
    memcpy(copy, src, len * sizeof(int));
    return copy;
}

void DrawGame()
{
    unsigned char *bmpBlock = (unsigned char *)&block[0];
    unsigned char *bmpClear = (unsigned char *)&clearblock[0];

    DrawShape(grid, 20, 10, 0, 0, bmpBlock, bmpClear);

    // Draw current shape
    int *shapeDef;
    int size = 0;
    if(shape == S_O)
    {
        size = 2;
        shapeDef = Copy(&SD_O[0], size*size);
    }
    else if(shape == S_I)
    {
        size = 4;
        shapeDef = Copy(&SD_I[0], size*size);
    }
    else if(shape == S_S)
    {
        size = 3;
        shapeDef = Copy(&SD_S[0], size*size);
    }
    else if(shape == S_Z)
    {
        size = 3;
        shapeDef = Copy(&SD_Z[0], size*size);
    }
    else if(shape == S_L)
    {
        size = 3;
        shapeDef = Copy(&SD_L[0], size*size);
    }
    else if(shape == S_J)
    {
        size = 3;
        shapeDef = Copy(&SD_J[0], size*size);
    }
    else if(shape == S_T)
    {
        size = 3;
        shapeDef = Copy(&SD_T[0], size*size);
    }

    if(size)
    {
        RotateShape(shapeDef, size);
        DrawShape(shapeDef, size, size, locationX, locationY, bmpBlock, bmpClear);
        free(shapeDef);
    }
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

    IntMasterEnable();
    while(1)
    {
        while(!tick); // Wait for events

        IntMasterDisable();
        tick = 0; // Reset event flag

        DrawGame();

        IntMasterEnable();
    }
}
