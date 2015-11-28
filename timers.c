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
int dropCounter = 0;

// Game state
int shape = -1;
int nextShape = -1;
int *shapeDef = NULL;
int *nextShapeDef = NULL;
int shapeDefSize = 0;
int nextShapeDefSize = 0;
int orientation = -1;
int locationX = -1;
int locationY = -1;
int grid[200] = { 0 };
int score = 0;
int tetris = 0;
int gameover = 0;

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
const int maxX = 9;
const int maxY = 19;

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

inline int ValidButtonCombo(int b0_t, int b1_t, int b2_t, int b3_t, int b4_t)
{
    if(b0_t)
    {
        return 0;
    }

    return b1_t + b2_t + b3_t + b4_t <= 1;
}

inline void Transpose(int *a, int *b, int *c, int *d)
{
   int temp = *a;
   *a = *b;
   *b = *c;
   *c = *d;
   *d = temp;
}

inline void Rotate(int *m, int n)
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

inline void RotateShape(int *m, int n, int newOrientation)
{
    int rotations =
        shape == S_O ? 0 :
        (shape == S_I || shape == S_S || shape == S_Z) && (newOrientation == O_000 || newOrientation == O_180) ? 0 :
        (shape == S_I || shape == S_S || shape == S_Z) ? 1 :
        newOrientation == O_000 ? 0 :
        newOrientation == O_090 ? 1 :
        newOrientation == O_180 ? 2 : 3;

    int i;
    for(i = 0; i < rotations; i++)
    {
        Rotate(m, n);
    }
}

inline int * Copy(const int *src, int len)
{
    int *copy = malloc(len * sizeof(int));
    memcpy(copy, src, len * sizeof(int));
    return copy;
}

inline int CheckPosition(int *shape, int shapeSize, int newX, int newY)
{
    int i, j;
    for(i = 0; i < shapeSize; i++)
    {
        for(j = 0; j < shapeSize; j++)
        {
            if(shape[i * shapeSize + j])
            {
                int thisY = newY + i;
                if(thisY < 0 || thisY > maxY)
                {
                    return 0;
                }

                int thisX = newX + j;
                if(thisX < 0 || thisX > maxX)
                {
                    return 0;
                }

                if(grid[((newY + i) * (maxX + 1)) + newX + j])
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}

inline int GetNextOrientation()
{
    if(orientation == O_000)
    {
        return O_090;
    }
    else if(orientation == O_090)
    {
        return O_180;
    }
    else if(orientation == O_180)
    {
        return O_270;
    }

    return  O_000;
}

inline int TryChangeOrientation()
{
    int next = GetNextOrientation();

    int *copy = Copy(&shapeDef[0], shapeDefSize * shapeDefSize);
    RotateShape(copy, shapeDefSize, next);

    if(CheckPosition(&copy[0], shapeDefSize, locationX, locationY))
    {
        free(shapeDef);
        shapeDef = copy;
        return 1;
    }

    free(copy);
    return 0;
}

inline int TryMove(int newX, int newY)
{
    if(CheckPosition(&shapeDef[0], shapeDefSize, newX, newY))
    {
        locationX = newX;
        locationY = newY;
        return 1;
    }
    return 0;
}

inline void StoreShape()
{
    int i, j;
    for(i = 0; i < shapeDefSize; i++)
    {
        for(j = 0; j < shapeDefSize; j++)
        {
            if(shapeDef[i * shapeDefSize + j])
            {
                grid[((locationY + i) * (maxX + 1)) + locationX + j] = 1;
            }
        }
    }
}

inline void RemoveLine(int lineIndex)
{
    int i, j;
    for(i = lineIndex; i > 0; i--)
    {
        for(j = 0; j <= maxX; j++)
        {
            grid[i * (maxX + 1) + j] = grid[(i - 1) * (maxX + 1) + j];
        }
    }

    // Clear top line
    for(j = 0; j < maxX; j++)
    {
        grid[j] = 0;
    }
}

inline int ClearLines()
{
    int linesRemoved = 0;

    int i, j;
    for(i = maxY; i >= 0; i--)
    {
        int removeLine = 1;
        for(j = 0; j <= maxX; j++)
        {
            if(!grid[i * (maxX + 1) + j])
            {
                removeLine = 0;
                break;
            }
        }

        if(removeLine)
        {
            RemoveLine(i);
            i++; // Reevaluate this line since it just changed
            linesRemoved++;
        }
    }

    return linesRemoved;
}

inline void UpdateScore(int numLines)
{
    if(numLines == 0)
    {
        score += 10;
        return;
    }
    if(numLines == 4)
    {
        if(tetris)
        {
            score += 1200;
        }
        else
        {
            tetris = 1;
            score += 800;
        }
        return;
    }

    tetris = 0;
    score += numLines * 100;
}

void InitShape(int s, int **buffer, int *size)
{
    if(*buffer)
    {
        free(*buffer);
    }

    if(s == S_O)
    {
        *size = 2;
        *buffer = Copy(&SD_O[0], *size * *size);
    }
    else if(s == S_I)
    {
        *size = 4;
        *buffer = Copy(&SD_I[0], *size * *size);
    }
    else
    {
        *size = 3;
        if(s == S_S)
        {
            *buffer = Copy(&SD_S[0], *size * *size);
        }
        else if(s == S_Z)
        {
            *buffer = Copy(&SD_Z[0], *size * *size);
        }
        else if(s == S_L)
        {
            *buffer = Copy(&SD_L[0], *size * *size);
        }
        else if (s == S_J)
        {
            *buffer = Copy(&SD_J[0], *size * *size);
        }
        else
        {
            *buffer = Copy(&SD_T[0], *size * *size);
        }
    }
}

void GetNextShape()
{
    int r = rand();

    if(!nextShapeDef)
    {
        nextShape = (r % 7) - 1;
    }

    shape = nextShape;
    nextShape = (r % 7) - 1;

    InitShape(shape, &shapeDef, &shapeDefSize);
    InitShape(nextShape, &nextShapeDef, &nextShapeDefSize);

    locationX = 4;
    locationY = 0;
    orientation = O_000;

    if(!TryMove(locationX, locationY))
    {
        gameover = 1;
    }
}

void Timer0IntHandler(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    if(gameover)
    {
        return;
    }

    if(!shapeDef)
    {
        GetNextShape();
        tick = 1;
    }

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
            if(TryChangeOrientation())
            {
                tick = 1;
                srand(dropCounter);
            }
        }
        if(ButtonDown(b2_t, b2) || b2_t && dropCounter % 10 == 0) // Throttle to 1/10th seconds
        {
            if(TryMove(locationX - 1, locationY))
            {
                tick = 1;
                srand(dropCounter);
            }
        }
        if(ButtonDown(b3_t, b3) || b3_t && dropCounter % 10 == 0)
        {
            if(TryMove(locationX + 1, locationY))
            {
                tick = 1;
                srand(dropCounter);
            }
        }
        if(b1_t) // Continuous press
        {
            if(TryMove(locationX, locationY + 1))
            {
                tick = 1;
            }
        }
    }

    // Store button states
    b0 = b0_t;
    b1 = b1_t;
    b2 = b2_t;
    b3 = b3_t;
    b4 = b4_t;

    dropCounter++;
    if(dropCounter == timerDivisor)
    {
        if(!TryMove(locationX, locationY + 1))
        {
            // Hit bottom or another piece
            StoreShape();
            int numLines = ClearLines();
            UpdateScore(numLines);

            free(shapeDef);
            shapeDef = NULL;
        }

        dropCounter = 0;
        tick = 1;
    }

    // Trigger the event the first time so the initial screen is drawn
    if(first)
    {
        first = 0;
        tick = 1; // Signal event
    }
}

inline char * IntToString(int input)
{
    // Convert int to char pointer
    // Only works with non-negative values 0 - 999999
    int flag = 0;
    int i = 5;
    char str[7] = { '0', '0', '0', '0', '0', '0', '\0' };
    while(input != 0)
    {
        str[i--] = (input % 10) + '0';
        input /= 10;
        flag = 1;
    }

    if(!flag) // Input == 0
    {
        return "000000";
    }

    char *result = malloc(7);
    if(result == NULL)
    {
        return NULL;
    }

    { // Additional scope so we don't need to define j up top, CC Studio is C89
        int j;
        for(j = 0; j < 7; j++)
        {
            result[j] = str[j];
        }
    }

    return result;
}

inline void DrawShape(int *m, int rows, int cols, int x, int y, unsigned char *bufferT, unsigned char *bufferF)
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
            else if(bufferF)
            {
                RIT128x96x4ImageDraw(bufferF, xAbs, yAbs, width, height);
            }
        }
    }
}

inline void DrawGame()
{
    unsigned char *bmpBlock = (unsigned char *)&block[0];
    unsigned char *bmpClear = (unsigned char *)&clear[0];

    DrawShape(grid, 20, 10, 0, 0, bmpBlock, bmpClear);

    if(shapeDef)
    {
        DrawShape(shapeDef, shapeDefSize, shapeDefSize, locationX, locationY, bmpBlock, NULL);
    }
    if(nextShapeDef)
    {
        DrawShape((int*)&SD_C[0], 4, 4, 13, -2, bmpBlock, bmpClear); // Clear previous
        DrawShape(nextShapeDef, nextShapeDefSize, nextShapeDefSize, 13, -2, bmpBlock, NULL);
    }

    char *scoreSt = IntToString(score);
    RIT128x96x4StringDraw("Score:", 0, 70, 15);
    RIT128x96x4StringDraw(scoreSt, 0, 80, 15);
    free(scoreSt);

    if(gameover)
    {
        RIT128x96x4StringDraw("   GAME OVER   ", 20, 48, 15);
    }
}

int main(void)
{
    // Init clocks
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_8MHZ);
    SysCtlPWMClockSet(SYSCTL_PWMDIV_8);

    // Init screen
    RIT128x96x4Init(1000000);
    unsigned char *bmpWall = (unsigned char *)&wall[0];
    RIT128x96x4ImageDraw(bmpWall, xOffset - 2, 0, 2, 96);
    RIT128x96x4ImageDraw(bmpWall, xOffset + 40, 0, 2, 96);

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
