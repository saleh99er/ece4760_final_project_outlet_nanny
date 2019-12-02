/*
 * File:        Main PIC32 code for IOT Power Monitor device
 * 
 * 
 * File:        Test of compiler fixed point
 * Author:      Bruce Land
 * Adapted from:
 *              main.c by
 * Author:      Syed Tahmid Mahbub
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2.h"

// need for rand function
#include <stdlib.h>
// fixed point types
#include <stdfix.h>
// math
#include <math.h>

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_cmd, pt_input, pt_output, pt_DMA_output;

// === the fixed point macros ========================================
typedef signed int fix16 ;
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b)))) 
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a)))) 
#define absfix16(a) abs(a)

//fix16 prev_adc_means[10];

// system 1 second interval tick
int sys_time_seconds ;

volatile unsigned int adc_raw;
volatile int point_num;
volatile int sum_squares = 0;
volatile int overflowCounter = 0;
volatile fix16 adc_mean;
#define ZERO_CURRENT_ADC_READING 775 //done experimentally with 2.5V voltage source

// === Timer Interrupt ==========================================
// == Timer 2 ISR =====================================================
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void){
    // clear the timer interrupt flag
    mT2ClearIntFlag();
    
    //compute rms of adc reading
    AcquireADC10();
    adc_raw = ReadADC10(0);
    adc_raw -= ZERO_CURRENT_ADC_READING;
    sum_squares += (adc_raw)*(adc_raw);
    if(sum_squares < 0) overflowCounter++;
    point_num += 1;
    
    if (point_num == 20){
        mPORTBToggleBits(BIT_0);
        // rms^2 stored in adc mean
        adc_mean = float2fix16((1.0*sum_squares) / 20);
        sum_squares = 0;
        point_num = 0;
    }
}

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1) {
        if(sys_time_seconds % 10 == 0){
            //mPORTBToggleBits(BIT_0);
        }
      // yield time 1 second
      PT_YIELD_TIME_msec(1000) ;
      sys_time_seconds++ ;

      // NEVER exit while
    } // END WHILE(1)
  PT_END(pt);
} // timer thread

static PT_THREAD (protothread_cmd(struct pt *pt))
{
    PT_BEGIN(pt);
    static char cmd[16]; 
    static int value_i;
    sprintf(PT_send_buffer, "PM PIC32 on");
    PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
      while(1) {
          PT_YIELD_TIME_msec(100);
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );             
            sscanf(PT_term_buffer, "%s %d", cmd, &value_i);
            //turn relay on/off
            if (cmd[0]=='r'){
                if (value_i == 0){
                    mPORTAClearBits(BIT_0);
                    mPORTBSetBits(BIT_2);
                }
                else{
                    mPORTASetBits(BIT_0);
                    mPORTBClearBits(BIT_2);
                }
                sprintf(PT_send_buffer,"r%d done", value_i);
                //sprintf(PT_send_buffer, "%s", PT_term_buffer); //send what was received
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //report rms current
            else if (cmd[0]=='i'){
                
            }
            else if (cmd[0]=='o'){
                sprintf(PT_send_buffer, "OC:%d", overflowCounter);
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //report raw adc reading after rms calculation
            else if (cmd[0]=='a'){
                //adc_raw
                //sprintf(PT_send_buffer, "%d", adc_raw);
                sprintf(PT_send_buffer, "%f", fix2float16(adc_mean));
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //unknown cmd
            else{
                sprintf(PT_send_buffer, "%s", "unknown cmd");
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
      } // END WHILE(1)
  PT_END(pt);
} // end of cmd thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
   //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 

  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();
  
        // the ADC ///////////////////////////////////////
        // configure and enable the ADC
	CloseADC10();	// ensure the ADC is off before setting the configuration

	// define setup parameters for OpenADC10
	// Turn module on | ouput in integer | trigger mode auto | enable autosample
        // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
        // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
        // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
        #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_OFF //

	// define setup parameters for OpenADC10
	// ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
	#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
        //
	// Define setup parameters for OpenADC10
        // use peripherial bus clock | set sample time | set ADC clock divider
        // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
        // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
        #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

	// define setup parameters for OpenADC10
	// set AN11 and  as analog inputs
	#define PARAM4	ENABLE_AN11_ANA  // pin 24 - RB13

	// define setup parameters for OpenADC10
	// do not assign channels to scan
	#define PARAM5	SKIP_SCAN_ALL

	// use ground as neg ref for A | use AN11 for input A     
	// configure to sample AN11 
	SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11 ); // configure to sample AN11 
	OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 ); // configure ADC using the parameters defined above

	EnableADC10(); // Enable the ADC
  ///////////////////////////////////////////////////////
    // === Config timer to ========
  // set up timer2 to generate the wave period
  // goal, sample 1,200 pts / sec
  // interrupt needs to happen every 8.3*10^-4 s or .83ms
  //  40 MHz  / 1.2kHz / 64 (prescalar) = 521 aprox
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_64, 521);                        //DEBUG
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);                                           //DEBUG
  mT2ClearIntFlag(); // and clear the interrupt flag                                     //DEBUG
  ANSELA = 0; ANSELB = 0; 
  
  //GPIO setup
  mPORTASetPinsDigitalOut(BIT_0);
  mPORTBSetPinsDigitalOut(BIT_2 | BIT_1 | BIT_0);
  
  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt_timer);
  PT_INIT(&pt_cmd);
  // seed random color
  srand(1);

  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      PT_SCHEDULE(protothread_cmd(&pt_cmd));
      }
  } // main

// === end  ======================================================

