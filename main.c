/*
 * Embedded II Project
 * adc seq samp 0
 *
 */
//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"
#include "adc0.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
#define mic1 PORTE,1 //AIN2
#define mic2 PORTE,2 //AIN1 4th mic
#define mic3 PORTE,3 //AIN0
#define mic4 PORTD,3 //AIN4

uint8_t count;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
// Initialize Hardware
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
    selectPinAnalogInput(mic4);

}
#define MAX_CHARS 80
char strInput[MAX_CHARS + 1];
char *token;
uint8_t count = 0;
void processShell(){
    bool end;
    char c;
    if (kbhitUart0())
        {
            c = getcUart0();

            end = (c == 13) || (count == MAX_CHARS);
            if (!end)
            {
                if ((c == 8 || c == 127) && count > 0)
                    count--;
                if (c >= ' ' && c < 127)
                    strInput[count++] = c;
            }
            else
            {
                strInput[count] = '\0';
                count = 0;
                token = strtok(strInput, " ");
                if (strcmp(token, "reboot") == 0)
                {
                    NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
                }

                if (strcmp(token, "help") == 0)
                {
                    putsUart0("Commands:\r");
                    putsUart0("  reboot\tReset hardware\r");
                    putsUart0("  average\tDisplay the average value (in DAC units and SPL) of each microphone\r");
                    putsUart0("  tc T\t\tSet the time constant of the average filter to T\r");
                    putsUart0("  backoff B\tSet the backoff between the first and the subsequent microphone signal threshold levels can be set\r");
                    putsUart0("  holdoff H\tSet the minimum time before the next event can be detected\r");
                    putsUart0("  hysteresis Y\tDetermine how much decrease in the average is required after an event before the next event can be detected\r");
                    putsUart0("  aoa\t\tReturn the most current angle of arrival\r");
                    putsUart0("  aoa always\tDisplay the AoA information of each event as it is detected\r");
                }

            }
        }
    }
void sampleMicrophoneIsr(){
    count++;
    int16_t value = readAdc0Ss1();
    char* test;
    sprintf(test,"Value: %d\r",count);
    putsUart0(test);
}
int main(void){

    count = 0;
    // Init controller
    initHw();
    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);
    //setup ADC0
    initAdc0Ss1();
    while(true){
        processShell();
    }

}


