/*
 * Embedded II Project
 * adc seq samp 0
 * Samuel Ruiz
 */
//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"
#include "adc0.h"
#include "nvic.h"
#include "math.h"
//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
#define mic1 PORTE,3 //AIN0
#define mic2 PORTE,1 //AIN2
#define mic3 PORTD,3 //AIN4

#define GREEN_LED PORTF,3
#define VRMS 0.00631

uint32_t isrCounter = 0;
uint8_t index = 0;

const uint16_t sampleBufferSize = 50;
uint16_t mic1Values[50];
//uint16_t mic2Average[];
uint16_t mic3Values[50];
uint16_t mic2Values[50];

uint16_t mic1Avg;
uint16_t mic2Avg;
uint16_t mic3Avg;

uint16_t mic1Event;
uint16_t mic2Event;
uint16_t mic3Event;

int16_t mic1Tdoa;
int16_t mic2Tdoa;
int16_t mic3Tdoa;

bool temp = true;
bool mic1Triggered;
bool mic2Triggered;
bool mic3Triggered;
uint16_t treshHold; //Set my uart to determine threshhold
uint16_t tempThreshHold; //Starts off at threshHold and lowers after first mic detection
uint32_t backoff;
uint32_t holdoff;
uint16_t tc; //time constant
uint16_t hysteresis;
int16_t aoa;
uint32_t holdOffTimer;
bool aoaAlways;
bool displayAverage;
bool firstTrigger;
bool tdoa;
bool fail;
#define MAX_CHARS 80
#define MAX_FIELDS 5
typedef struct _USER_DATA
{
    char buffer[MAX_CHARS + 1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

USER_DATA data;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
// Initialize Hardware
char* getFieldString(USER_DATA *data, uint8_t fieldNumber)
{
    return &(data->buffer[data->fieldPosition[fieldNumber]]);
}

int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber)
{
    return atoi(&(data->buffer[data->fieldPosition[fieldNumber]]));
}

bool isCommand(USER_DATA *data, const char strCommand[], uint8_t minArguments)
{
    if (strcmp(data->buffer, strCommand) == 0)
    {
        if (data->fieldCount - 1 == minArguments)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

void getsUart0(USER_DATA *data)
{
    uint8_t count = 0;
    while (true)
    {
        char c = getcUart0();               //Gets next char
        if (c == 8 || c == 127)
        {             // If c is backspace
            if (count > 0)
            {
                count--;
            }
        }
        else if (c == 13)
        {                   //if c is enter
            data->buffer[count] = '\0';
            break;
        }
        else if (c >= 32)
        {                   //anything else gets added to the buffer
            data->buffer[count] = c;
            count++;
            if (count == MAX_CHARS)
            {
                data->buffer[count] = '\0';
                break;
            }
        }
    }

    return;
}

void parseFields(USER_DATA *data)
{
    int8_t count = 0;
    int8_t fieldCount = 0;
    char type;
    bool isCurrentDelimiter = false;
    bool isPastDelimiter = true;
    while (count != MAX_CHARS || fieldCount <= MAX_FIELDS)
    {

        if (data->buffer[count] == '\0')
        {
            break;
        }
        else if ((data->buffer[count]) >= 65 && (data->buffer[count]) <= 90) //A-Z
        {
            type = 'a';
            isCurrentDelimiter = false;
        }
        else if ((data->buffer[count]) >= 97 && (data->buffer[count]) <= 122) //a - z
        {
            type = 'a';
            isCurrentDelimiter = false;
        }
        else if ((data->buffer[count]) >= 45 && (data->buffer[count]) <= 57) //0-9-.
        {
            type = 'n';
            isCurrentDelimiter = false;
        }
        else
        {
            isCurrentDelimiter = true;
        }

        if (isCurrentDelimiter)
        {
            isPastDelimiter = true;
        }
        else if (isCurrentDelimiter == false && isPastDelimiter == true)
        { //checks if current is not delimiter
            data->fieldPosition[fieldCount] = count;     //and if past is, saves
            data->fieldType[fieldCount] = type;
            fieldCount++;
            data->fieldCount = fieldCount;
            isPastDelimiter = false;
            data->buffer[count - 1] = NULL;
        }
        count++;
    }
}
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    enablePort(PORTE);
    enablePort(PORTD);
    _delay_cycles(3);

    selectPinAnalogInput(mic1);
    selectPinAnalogInput(mic2);
    selectPinAnalogInput(mic3);

    selectPinPushPullOutput(GREEN_LED);

}
void sampleMicrophoneIsr()
{
    //Heartbeat LED runs every second
    if (isrCounter >= 333333)
    {
        temp ^= 1;
        setPinValue(GREEN_LED, temp);
        //If first mic is triggered being counting until reset time
        isrCounter = 0;
    }
    char str[100];
    //ADC0 ISR reads the mics one at a time and checks if they fit the treshhold
    //Every adc read takes approximately 1 ms which gives a time of how long it takes
    //for sound to reach all mics
    uint16_t mic1Temp = readAdc0Ss1();
    //Treshold
    if ((mic1Temp >= (tempThreshHold + mic1Avg)) && !mic1Triggered)
    {
        if (!firstTrigger)
        {
            tempThreshHold = treshHold - backoff;
            firstTrigger = true;
        }
        mic1Triggered = true;
        mic1Event = mic1Temp;
        mic1Tdoa = holdOffTimer;

    }
    uint16_t mic2Temp = readAdc0Ss1();
    if ((mic2Temp >= (tempThreshHold + mic2Avg)) && !mic2Triggered)
    {
        if (!firstTrigger)
        {
            tempThreshHold = treshHold - backoff;
            firstTrigger = true;
        }
        mic2Triggered = true;
        mic2Event = mic2Temp;
        mic2Tdoa = holdOffTimer;

    }
    uint16_t mic3Temp = readAdc0Ss1();
    if ((mic3Temp >= (tempThreshHold + mic3Avg)) && !mic3Triggered)
    {
        if (!firstTrigger)
        {
            tempThreshHold = treshHold - backoff;
            firstTrigger = true;
        }
        mic3Triggered = true;
        mic3Event = mic3Temp;
        mic3Tdoa = holdOffTimer;
    }
    if ((isrCounter % 10) == 0) //Every other sample store the values in the array
    {
        mic1Values[index] = mic1Temp;
        mic2Values[index] = mic2Temp;
        mic3Values[index] = mic3Temp;
        //Circular buffer
        if (index <= sampleBufferSize)
        {
            index++;
        }
        else
        {
            index = 0;
        }
    }
    //Hold off time has expired
    if (holdOffTimer >= holdoff)
    {
        //If all mics were triggered by expiration holdoff time
        if (mic1Triggered && mic2Triggered && mic3Triggered)
        {
            //If user has turned on ToA display
            if (tdoa)
            {
                sprintf(str, "\nMic1 ToA: %d\nMic2 ToA: %d\nMic3 ToA: %d\n",
                        mic1Tdoa, mic2Tdoa, mic3Tdoa);
                putsUart0(str);

            }
            double k;
            int16_t t0,t1,t2, theta;
            if (mic1Tdoa == 0)
            {
//                aoa = 0.95 * ((mic2Tdoa - mic1Tdoa) - (mic3Tdoa - mic1Tdoa))
//                        + 120;

                theta = 120;

                if(mic2Tdoa >= mic3Tdoa){
                    k = 2;
                    t1 = (mic2Tdoa - mic1Tdoa);
                    t2 = (mic3Tdoa - mic1Tdoa);
                }
                else{
                    k = 1.5;
                    t1 = (mic3Tdoa - mic1Tdoa);
                    t2 = (mic2Tdoa - mic1Tdoa);
                }
            }
            else if (mic2Tdoa == 0)
            {
//                aoa = 1.08 * ((mic3Tdoa - mic2Tdoa) - (mic1Tdoa - mic2Tdoa))
//                        + 240;
                k = 1.7;
                theta = 240;

                if(mic3Tdoa >= mic1Tdoa){
                    t1 = (mic3Tdoa - mic2Tdoa);
                    t2 = (mic1Tdoa - mic2Tdoa);
                }
                else{
                    t1 = (mic1Tdoa - mic2Tdoa);
                    t2 = (mic3Tdoa - mic2Tdoa);
                }
            }
            else if (mic3Tdoa == 0)
            {
//                aoa = .95 * ((mic2Tdoa - mic3Tdoa) - (mic1Tdoa - mic3Tdoa)) + 0;

                k = 1.25;

                if(mic2Tdoa >= mic1Tdoa){
                    t2 = (mic2Tdoa - mic3Tdoa);
                    t1 = (mic1Tdoa - mic3Tdoa);
                    theta = 0;
                }
                else{
                    t1 = (mic1Tdoa - mic3Tdoa);
                    t2 = (mic2Tdoa - mic3Tdoa);
                    theta = 360;
                }
            }
            aoa =(k*(t2-t1))+theta;
            if (aoaAlways)
            {
                char str[100];
                sprintf(str, "aoa: %d\n", aoa);
                putsUart0(str);
            }
        }
        //If one or more mics was not a qualified event
        else
        {
            //If user has turned on fail display
            if (fail)
            {
                sprintf(str,
                        "\nMic1 ToA fail: %d\nMic2 ToA Fail: %d\nMic3 ToA Fail: %d\n",
                        mic1Tdoa, mic2Tdoa, mic3Tdoa);
                putsUart0(str);
            }
        }
        tempThreshHold = treshHold; //Every second resets the threshHold back to default
        mic1Triggered = false;
        mic2Triggered = false;
        mic3Triggered = false;
        firstTrigger = false;
        holdOffTimer = 0;
    }

    ADC0_ISC_R |= ADC_ISC_IN1;
    if (firstTrigger)
    {
        holdOffTimer++;

    }
    isrCounter++;
//    if (kbhitUart0())
//    {
//        disableNvicInterrupt(31);
//    }
}

void getAverage()
{

    uint8_t x;
    uint16_t micTemp1 = 0;
    uint16_t micTemp3 = 0;
    uint16_t micTemp2 = 0;
    char str[100];
    //Every quarter second get the average
    if (isrCounter % tc == 0)
    {
        for (x = 0; x <= sampleBufferSize; x++)
        {
            micTemp1 += mic1Values[x];
            micTemp2 += mic2Values[x];
            micTemp3 += mic3Values[x];
        }
        mic1Avg = (micTemp1) / (sampleBufferSize + 1);
        mic2Avg = (micTemp2) / (sampleBufferSize + 1);
        mic3Avg = (micTemp3) / (sampleBufferSize + 1);
    }

}

int main(void)
{

    // Init controller
    initHw();
    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);
    //setup ADC0
    initAdc0Ss1();
    enableNvicInterrupt(31);
    index = 0;
    isrCounter = 0;
    //default start high and work way down
    mic1Avg = 1000;
    mic2Avg = 1000;
    mic3Avg = 1000;
    tempThreshHold = treshHold;
    mic1Triggered = false;
    mic2Triggered = false;
    mic3Triggered = false;
    firstTrigger = false;

    aoaAlways = true;
    displayAverage = true;
    tdoa = true;
    fail = false;

    treshHold = 1000; //Set my uart to determine threshhold
    tempThreshHold = treshHold; //Starts off at threshHold and lowers after first mic detection
    backoff = 550;
    holdoff = 333333; //resets every second or so
    holdOffTimer = 0;
    tc = 100; //time constant
    hysteresis = 0;
    while (true)
    {
        if (!firstTrigger)
        {
            getAverage();
        }
//        ----------------------------------------------------------
//        UART COMMANDS
        if (kbhitUart0())
        {
            disableNvicInterrupt(31);
            getsUart0(&data);
            parseFields(&data);
            if (isCommand(&data, "help", 0))
            {
                putsUart0("Commands:\r");
                putsUart0("  reset\tReset hardware\r");
                putsUart0(
                        "  average\tDisplay the average value (in DAC units and SPL) of each microphone\r");
                putsUart0(
                        "  tc T\t\tSet the time constant of the average filter to T\r");
                putsUart0(
                        "  backoff B\tSet the backoff between the first and the subsequent microphone signal threshold levels can be set\r");
                putsUart0(
                        "  holdoff H\tSet the minimum time before the next event can be detected\r");
                putsUart0(
                        "  hysteresis Y\tDetermine how much decrease in the average is required after an event before the next event can be detected\r");
                putsUart0(
                        "  aoa\t\tReturn the most current angle of arrival\r");
                putsUart0(
                        "  aoa always\tDisplay the AoA information of each event as it is detected\r");
            }
            else if (isCommand(&data, "reset", 0))
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }
            else if (isCommand(&data, "average", 0))
            {
                char str[100];
                double  dac1 = ((float)mic1Avg/4096)*3.3;
                double  dac2 = ((float)mic2Avg/4096)*3.3;
                double  dac3 = ((float)mic3Avg/4096)*3.3;

                float spl1 = (20*log10(dac1/VRMS)) - 44 + 94 - 40;
                float spl2 = (20*log10(dac2/VRMS)) - 44 + 94 - 40;
                float spl3 = (20*log10(dac3/VRMS)) - 44 + 94 - 40;

                sprintf(str, "Average: Mic 1 DAC:%0.2f SPL:%0.2f\tMic 2 DAC:%0.2f SPL:%0.2f\tMic 3 DAC:%0.2f SPL:%0.2f\n",
                                        dac1,spl1,dac2,spl2, dac3,spl3);
                putsUart0(str);
            }
            else if (isCommand(&data, "tc", 0))
            {
                char str[100];
                sprintf(str, "tc: %d\n", tc);
                putsUart0(str);
            }
            else if (isCommand(&data, "tc", 1))
            {
                tc = getFieldInteger(&data, 1);
            }
            else if (isCommand(&data, "backoff", 0))
            {
                char str[100];
                sprintf(str, "b: %d\n", backoff);
                putsUart0(str);
            }
            else if (isCommand(&data, "backoff", 1))
            {
                backoff = getFieldInteger(&data, 1);
            }
            else if (isCommand(&data, "holdoff", 0))
            {
                char str[100];
                sprintf(str, "holdoff: %dsec\n", holdoff / 333333);
                putsUart0(str);
            }
            else if (isCommand(&data, "holdoff", 1))
            {
                holdoff = getFieldInteger(&data, 1) * 333333;
            }
            else if (isCommand(&data, "hysteresis", 0))
            {
                char str[100];
                sprintf(str, "hysteresis: %d\n", tc);
                putsUart0(str);
            }
            else if (isCommand(&data, "hysteresis", 1))
            {
                hysteresis = getFieldInteger(&data, 1);
            }
            else if (isCommand(&data, "aoa", 0))
            {
                char str[100];
                sprintf(str, "aoa: %d\n", aoa);
                putsUart0(str);
            }
            else if (isCommand(&data, "aoa", 1))
            {
                char *aoaAlwaysRead = getFieldString(&data, 1);
                if (strcmp(aoaAlwaysRead, "always") == 0)
                {
                    aoaAlways = !aoaAlways;
                }

            }
            else if (isCommand(&data, "tdoa", 1))
            {
                char *tdoaRead = getFieldString(&data, 1);
                if (strcmp(tdoaRead, "ON") == 0)
                {
                    tdoa = true;
                }
                else if (strcmp(tdoaRead, "OFF") == 0)
                {
                    tdoa = false;
                }
            }
            else if (isCommand(&data, "fail", 1))
            {
                char *failRead = getFieldString(&data, 1);
                if (strcmp(failRead, "ON") == 0)
                {
                    fail = true;
                }
                else if (strcmp(failRead, "OFF") == 0)
                {
                    fail = false;
                }
            }
            else
            {
                putsUart0("Invalid Command\n");
                putcUart0('\n');
            }
        }
        else
        {
            enableNvicInterrupt(31);
        }
    }
}

