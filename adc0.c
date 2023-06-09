// ADC0 Library
// Jason Losh
//modified by Sam Ruiz for Embedded II project
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// ADC0 SS1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "adc0.h"

#define ADC_CTL_DITHER          0x00000040

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initAdc0Ss1()
{
    // Enable clocks
    SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R0;
    _delay_cycles(16);

    // Configure ADC
    ADC0_ACTSS_R &= ~ADC_ACTSS_ASEN1;                // disable sample sequencer 1 (SS1) for programming
    ADC0_CC_R = ADC_CC_CS_SYSPLL;                    // select PLL as the time base (not needed, since default value)
    ADC0_PC_R = ADC_PC_SR_1M;                        // select 1msps rate
    ADC0_EMUX_R = ADC_EMUX_EM1_ALWAYS;            // select SS3 bit in ADCPSSI as trigger
    ADC0_SSCTL1_R |= ADC_SSCTL1_END2 | ADC_SSCTL1_IE2; // End after 3 samples, int after 3 samples
    ADC0_SSMUX1_R  = 0;                           // Set analog input for mic 1, PORTE,3 at AIN0
    ADC0_SSMUX1_R |= 2<<4;                           // Set analog input for mic 2, PORTE,1 at AIN2
    ADC0_SSMUX1_R |= 4<<8;                           // Set analog input for mic 3, PORTD,3 at AIN4
    ADC0_SAC_R = 0;
    ADC0_IM_R |= ADC_IM_MASK1;
    ADC0_ACTSS_R |= ADC_ACTSS_ASEN1;                 // enable SS1 for operation
}


// Request and read one sample from SS1
int16_t readAdc0Ss1()
{
    //while (ADC0_ACTSS_R & ADC_ACTSS_BUSY);           // wait until SS1 is not busy
    //while (ADC0_SSFSTAT1_R & ADC_SSFSTAT1_EMPTY);
    return ADC0_SSFIFO1_R;
}
