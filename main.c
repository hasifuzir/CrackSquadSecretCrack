
/*******************************************************************************
 * Template for Real-time Audio Descrambler, Second Year Scenario.
 *
 * This template is provided for your use in the Real-time Audio Descrambler Scenario.
 *
 * The template sets up all the important peripherals on the MSP432 for you.  This includes setting up the 48 MHz
 * high-frequency crystal (HFXT), the master clock MCLK to run at 48 MHz, the internal core voltage for 48 MHz
 * operation, the ADC reference voltages and outputting them to P5.6 and P5.7.
 * However, you need to set up the ADC yourself.
 *
 * Most importantly, the template sets up the timer SysTick to count 960 clock cycles.  Every time 960 clock cycles
 * or periods have passed, SysTick will generate an interrupt and the interrupt service routine systick_isr will be called.
 * Since the master clock is 48 MHz, systick_isr will be called 50,000 times a second (48 MHz / 960 = 50,000) or 50 kHz.
 * Any commands or functions inside systick_isr will therefore be executed at 50 kHz which can be used as
 * your sampling frequency.
 *
 * The descrambling and filtering functions should be implemented inside the systick_isr as these are
 * related to and run at the sampling frequency, i.e. 50 kHz.
 *
 * Anything else that only needs to be run once but not repeatedly should be implemented inside the function main{}.
 *
 * You need to make your codes as efficient as possible.  Any codes you put in systick_isr must be completed
 * before the next interrupt is generated.  To see how long it will take to perform all the functions inside systick_isr,
 * in this template, P6.0 is set to high at the start of systick_isr.  At the end of systick_isr, P6.0 is set to low.
 * You can therefore use an oscilloscope to measure the output at P6.0 to see how long it takes the systick_isr to
 * complete all the codes there.
 *
 * You should save the whole template folder in the workspace folder of the Code Composer Studio (CCS).  Then within CCS,
 * "import" this project.
 *
 *
 *
 * By Dr Chin-Pang Liu, University College London, 2015
 ******************************************************************************/


/* DriverLib Includes */
#include "driverlib.h" // This library is required if you want to use the library functions such as "MAP_GPIO_setAsPeripheralModuleFunctionInputPin"

/* Standard Includes already here from the example*/
#include <stdint.h>
#include <stdbool.h>

/* Maths Includes */
#include <math.h>  // This library is required to use the "sin" function

#include "filter.h"

#define PI 3.14159265f

//=---=--=--=
#include <stdio.h>
//=-=-=-=-=--=


int main(void)
{
    /* Halting the Watchdog */
    MAP_WDT_A_holdTimer();  // This command is used in almost all programs to stop the MSP from stopping automatically.


    /* P6.0 set as output.  This is used for timing the duration of the systick_isr.  P6.0 is set high on entering the systick_isr and
     * off on exiting.  An oscilloscope can be used to monitor how much time the interrupt requires to complete all the operations.
     * It can also measure the frequency at which the isr is called.  This should be 50 kHz*/
    P6DIR |= BIT0;
    P1DIR |= BIT0;


    /* Configure P5.6 to its analog function to output VREF.  VREF is set to 1.2V below and so the ADC input voltage should be between 0 V and 1.2 V */
    	P5SEL0 |= BIT6 | BIT7; // Set pins P5.6 and P5.7 as external reference voltage. See section 10.2.6 in slau356a.pdf, tables 4.1 and 6.45 of msp432p401r.pdf, slau596.pdf.
        P5SEL1 |= BIT6 | BIT7; // Set pins P5.6 and P5.7 as external reference voltage. See section 10.2.6 in slau356a.pdf, tables 4.1 and 6.45 of msp432p401r.pdf, slau596.pdf.


        REFCTL0 |= REFON;                     // Turn on reference module.  Section 19.3.1 in slau365a.pdf.
        REFCTL0 |= REFOUT;                    // Output reference voltage to a pin.  Section 19.3.1 in slau365a.pdf.


     /* Output VREF = 1.2V */
        REFCTL0 &= ~(REFVSEL_3);              // Clear existing VREF voltage level setting. Table 19-2 in slau365a.pdf.
        REFCTL0 |= REFVSEL_0;                 // Set VREF = 1.2V. Table 19-2 in slau365a.pdf.
        while (REFCTL0 & REFGENBUSY);       // Wait until the reference generation is settled.  Table 19-2 in slau365a.pdf.


    /* Configuring pins for high frequency crystal (HFXT) crystal for 48 MHz clock */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_PJ,
            GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION); //  Section 10.4.2.13 in MSP432_DriverLib_Users_Guide.  In Figure 4.1 in msp432p401r.pdf, you can see that the HFXT is conneccted to pins 2 and 3 of Port J.

    //-=-=-==--==-=-=-=-=-=--LIGHTS-=-=ADC
        /* Set P6.0 as output */
        MAP_GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN0);

        /* Set all 8 pins of P2 as output */
        MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
    //==-=-=-=--=-==-=-=-=-


    /* Set P4.3 to output the internal 48 MHz master clock as (MCLK) its primary function so you can check with a scope on this pin to make sure the clock is indeed 48 MHz */
       MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN3,
       GPIO_PRIMARY_MODULE_FUNCTION);


    /* There is a 32 kHz low frequency crystal (LFXT) and a 48 MHz high frequency crystal (HFXT) on the Launchpad. This command tells the MSP432 what frequencies these two external crystals are*/
    /* Section 6.6.2.4 in MSP432_DriverLib_Users_Guide*/
       CS_setExternalClockSourceFrequency(32000,48000000);


    /* Starting HFXT in non-bypass mode without a timeout.
     * Before increasing MCLK to a higher speed, it is necessary for software to ensure that the CPU voltage or core voltage (VCORE level) is
     * sufficiently high for the chosen frequency.  This is done through the Power Control Manager (PCM).  See Chapter 7 in slau356a.pdf.
     * To run the CPU at the maximum frequency of 48 MHz, the core voltage must be set to VCORE1.  See section 7.4.1 in slau356a.pdf.
     * It is much easier to use the library commands below to set the core voltage to VCORE1.  */
    MAP_PCM_setCoreVoltageLevel(PCM_VCORE1);  // Section 14.7.2.16 in MSP432_DriverLib_Users_Guide.
    MAP_FlashCtl_setWaitState(FLASH_BANK0, 2);
    MAP_FlashCtl_setWaitState(FLASH_BANK1, 2);
    CS_startHFXT(false);  // Initialise the HFXT.  "false" means we are not using the bypass mode but are using the crystal.  Section 6.6.2.27 in MSP432_DriverLib_Users_Guide.


    /* Initializing the master clock (MCLK) to HFXT (effectively 48MHz) */
    MAP_CS_initClockSignal(CS_MCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);  // The divider is set to 1 and so the MCLK is the same as the HFXT at 48 MHz.  Section 6.6.2.18 in in MSP432_DriverLib_Users_Guide.


    /* Configuring the timer SysTick to trigger at 50 kHz which will be the sampling frequency for sampling and processing the audio signal.
     * See Chapter 22 in MSP432_DriverLib_Users_Guide.
     * The other two timers "Timer32" and "Timer_A" can also be used instead */
    MAP_SysTick_enableModule();  //Enable the timer SysTick
    MAP_SysTick_setPeriod(960);  // SysTick will count 960 MCLK cycle before generating and calling an interrupt.  Therefore the sampling frequency is 48 MHz / 960 = 50 kHz
    MAP_Interrupt_enableSleepOnIsrExit();  //Enables the processor to sleep when exiting an ISR. For low power operation, this is ideal as power cycles are not wasted with the processing required for waking up from an ISR and going back to sleep.
    MAP_SysTick_enableInterrupt();  // Enable timer interrupt.

    //=-=-=-=-=--==-=-=-=-=
        /* Initializing ADC (MCLK/1//1) */
            MAP_ADC14_enableModule();
            MAP_ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_1,
            0);

            /* Configuring GPIOs (P5.0 (A5) as the ADC input pin) */
            MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5, GPIO_PIN0,
            GPIO_TERTIARY_MODULE_FUNCTION);

            /* Configure ADC Resolution */
            ADC14_setResolution(ADC_10BIT);


            /* Configuring ADC Memory */
            MAP_ADC14_configureSingleSampleMode(ADC_MEM0, true);
            MAP_ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_INTBUF_VREFNEG_VSS,
            ADC_INPUT_A5, false);


            /* Configuring Sample Timer */
            MAP_ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);
        //==--=--==-=-=-=--=


     /* Enabling MASTER interrupts */
    MAP_Interrupt_enableMaster();

	
	
	
    while (1)
    {
        //MAP_PCM_gotoLPM0();
    }

}



/* The following systick_isr is called 50000 times a second, i.e. 50 kHz, controlled by the master clock and SysTick  */
void systick_isr(void)
{
	static int i = 0;
	P6OUT |= BIT0; // set P6.0 high on entering this interrupt service routine (isr). Include your codes below
		i = i+1;
	//-=-=-=-=-=ADC
	static long temp_ADC = 0;


		temp_ADC = ADC14->MEM[0];  //Get the conversion result.  Alternatively, you can use temp_ADC = ADC14_getResult(ADC_MEM0)
		P2OUT = temp_ADC / 4;  //We do this because the ADC is set to use 10 bits but P2OUT is only 8 bits.

	    if(i==2){
	//    	float voltage = temp_ADC/1023 * 3.3;
	    	printf("%ld\n",temp_ADC);
	    	i=0;
	    }

		/* Enabling/Toggling Conversion */
		MAP_ADC14_enableConversion();
		MAP_ADC14_toggleConversionTrigger();
		MAP_ADC14_toggleConversionTrigger();
	//--==-=-=-=-





	P1OUT |= BIT0;

	float input = ;
	
	float input_bandstop = bandstop(input);


	P6OUT &= ~BIT0; // set P6.0 low on exiting this interrupt service routine (isr). Include yours codes above
}




/* ADC Interrupt Handler. This handler is called whenever there is a conversion
* that is finished for ADC_MEM0.
*/
void adc_isr(void)
{

	uint64_t status = MAP_ADC14_getEnabledInterruptStatus();
	MAP_ADC14_clearInterruptFlag(status);
	/* This simply clears the ADC14 interrupt and exits this adc_isr. */

}


