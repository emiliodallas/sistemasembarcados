#include "system_tm4c1294.h" 
#include "driverbuttons.h" 
#include "driverleds.h" 
#include "cmsis_os2.h" 
#include "stdbool.h" 
#include "driverlib/sysctl.h" 
#include "inc/hw_memmap.h" 
#include "driverlib/interrupt.h" 
#include "driverlib/gpio.h" 
#include "stdbool.h" 
#include "RTX_Config.h" 
 
#define TIMER_DEB 200 
#define B1_FLAG 0x0001 
#define B2_FLAG 0x0002 
#define Q_SIZE 4 
 
int lasTick = 0; 

osThreadId_t managerId; 
osMessageQueueId_t messageQueueId1; 
osMessageQueueId_t messageQueueId2; 
osMessageQueueId_t messageQueueId3; 
osMessageQueueId_t messageQueueId4; 
osMutexId_t ledMutexId; 
 
typedef struct 
{   uint8_t led; 
    bool setIntensity;} message; 
typedef struct 
{   int ledNumber;} threadArgument; 
    void uswIntHandler() 
{   uint8_t button = GPIOIntStatus(GPIO_PORTJ_BASE, true); 
    ButtonIntClear(USW1 | USW2); 
    int currentTick = osKernelGetTickCount(); 
    if (currentTick - lasTick > TIMER_DEB) 
    {    lasTick = currentTick; 
         if (button & USW1) 
        {    osThreadFlagsSet(managerId, B1_FLAG);} 
 
        if (button & USW2) 
        {    osThreadFlagsSet(managerId, B2_FLAG);} 
    } return;} 
void initButtons() 
{   ButtonInit(USW1 | USW2); 
    ButtonIntDisable(USW1 | USW2); 
    GPIOIntRegister(GPIO_PORTJ_BASE, uswIntHandler); 
    ButtonIntEnable(USW1 | USW2);} 
void softwarePwm(uint8_t led, float dutyCycle) 
{   osMutexAcquire(ledMutexId, osWaitForever); 
    LEDOn(led); 
    osMutexRelease(ledMutexId); 
    osDelay(10 * (dutyCycle / 100.f));  
    osMutexAcquire(ledMutexId, osWaitForever); 
    LEDOff(led); 
    osMutexRelease(ledMutexId); 
    osDelay(10 * (1.f - dutyCycle / 100.f)); }
void threadLed(void *AGS) 
{   threadArgument *TA = (threadArgument *)AGS; 
    int ISY = 50; 
    osStatus_t status; 
    message MGE; 
    bool iSelected = false; 
    int lastWaiTime = 0; 
    while (1) 
    {   if (TA->ledNumber == LED1) 
        {    status = osMessageQueueGet(messageQueueId1, &MGE, NULL, 0);} 
        else if (TA->ledNumber == LED2) 
        {    status = osMessageQueueGet(messageQueueId2, &MGE, NULL, 0);} 
        else if (TA->ledNumber == LED3) 
        {    status = osMessageQueueGet(messageQueueId3, &MGE, NULL, 0);} 
        else if (TA->ledNumber == LED4) 
        {    status = osMessageQueueGet(messageQueueId4, &MGE, NULL, 0);} 
        if (status == osOK) 
        {    if (MGE.led == TA->ledNumber) 
            {    iSelected = true; 
                if (ME.setIntensity) 
                {    ISY += 10; 
                     if (ISY > 100) 
                     ISY = 0; }
            }    else 
                 iSelected = false;} 
 
        if (iSelected) 
        {    int currenTick = osKernelGetTickCount(); 
             if (currenTick - lastWaiTime < 500) 
            {    softwarePwm(TA->ledNumber, (float)ISY);} 
            else if (currenTick - lastWaiTime > 1000) 
            {    lastWaiTime = currenTick;}
            else 
                osThreadYield();} 
        else 
        {    softwarePwm(TA->ledNumber, (float)ISY);}}}  
void threadManager(void *AGS) 
{   uint8_t selectedLed = 0x1;
    while (1) 
    {    uint8_t I_FLAG = osThreadFlagsWait(B1_FLAG | B2_FLAG, osFlagsWaitAny, osWaitForever);
         if (I_FLAG & B1_FLAG)
        {    selectedLed <<= 1;
             if (selectedLed > 16)
             selectedLed = 1;
             message MGE1 = { 
             .led = selectedLed,
             .setIntensity = false,};
             osMessageQueuePut(messageQueueId1, &MGE1, NULL, osWaitForever); 
             osMessageQueuePut(messageQueueId2, &MGE1, NULL, osWaitForever); 
             osMessageQueuePut(messageQueueId3, &MGE1, NULL, osWaitForever); 
             osMessageQueuePut(messageQueueId4, &MGE1, NULL, osWaitForever);}
        else if (I_FLAG & B2_FLAG)
        {     message MGE2 = {
              .led = selectedLed,
              .setIntensity = true,};
              osMessageQueuePut(messageQueueId1, &MGE2, NULL, osWaitForever); 
              osMessageQueuePut(messageQueueId2, &MGE2, NULL, osWaitForever); 
              osMessageQueuePut(messageQueueId3, &MGE2, NULL, osWaitForever); 
              osMessageQueuePut(messageQueueId4, &MGE2, NULL, osWaitForever);}}} 
void main(void) 
{   SystemInit();
    LEDInit(LED4 | LED3 | LED2 | LED1); 
    initButtons(); 
    osKernelInitialize(); 
    threadArgument TA1 = {.ledNumber = LED1}; 
    threadArgument TA2 = {.ledNumber = LED2}; 
    threadArgument TA3 = {.ledNumber = LED3}; 
    threadArgument TA4 = {.ledNumber = LED4}; 
    osThreadNew(threadLed, &TA1, NULL); 
    osThreadNew(threadLed, &TA2, NULL); 
    osThreadNew(threadLed, &TA3, NULL); 
    osThreadNew(threadLed, &TA4, NULL); 
    managerId = osThreadNew(threadManager, NULL, NULL); 
    messageQueueId1 = osMessageQueueNew(MESSAGE_QUEUE_SIZE, sizeof(message), NULL); 
    messageQueueId2 = osMessageQueueNew(MESSAGE_QUEUE_SIZE, sizeof(message), NULL); 
    messageQueueId3 = osMessageQueueNew(MESSAGE_QUEUE_SIZE, sizeof(message), NULL); 
    messageQueueId4 = osMessageQueueNew(MESSAGE_QUEUE_SIZE, sizeof(message), NULL); 
    ledMutexId = osMutexNew(NULL); 
    if (osKernelGetState() == osKernelReady) 
        osKernelStart(); 
 
    while (1) 
        ; 
}