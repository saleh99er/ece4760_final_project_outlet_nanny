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
static struct pt pt_timer, pt_cmd, pt_fft, pt_input, pt_output, pt_DMA_output;

// === the fixed point macros ========================================
typedef signed int fix16; //for ???
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b)))) 
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a)))) 
#define absfix16(a) abs(a)
typedef signed short fix14; //for fft
#define multfix14(a,b) ((fix14)((((long)(a))*((long)(b)))>>14)) //multiply two fixed 2.14
#define float2fix14(a) ((fix14)((a)*16384.0)) // 2^14
#define fix2float14(a) ((float)(a)/16384.0)
#define int2fix14(a) ((fix14)(a<<14))
#define absfix14(a) abs(a) 

// FFT
#define N_WAVE          512    /* size of FFT 512 */
#define LOG2_N_WAVE     9     /* log2(N_WAVE) 0 */
fix14 v_in[N_WAVE] ;

fix14 Sinewave[N_WAVE]; // a table of sines for the FFT
fix14 window[N_WAVE]; // a table of window values for the FFT
fix14 fr[N_WAVE], fi[N_WAVE];

#define SIXTY_HZ 30
#define AMP_TO_RMS 0.707
#define CUR_SENSOR_VOUT_TO_AMP 10
//#define CUR_SENSOR_RMS (AMP_TO_RMS*CUR_SENSOR_VOUT_TO_AMP)
#define CUR_SENSOR_RMS 7.07
fix14 current;
fix14 currentLimit;

// system 1 second interval tick
int sys_time_seconds ;

volatile unsigned int adc_raw;

// === Timer Interrupt ==========================================

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
    static float value_f;
    sprintf(PT_send_buffer, "PM PIC32 on");
    PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
      while(1) {
          PT_YIELD_TIME_msec(100);
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );             
            sscanf(PT_term_buffer, "%s %d", cmd, &value_i);
            sscanf(PT_term_buffer, "%s %f", cmd, &value_f);
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
                sprintf(PT_send_buffer, "%f", fix2float14(current));
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //report average power
            else if (cmd[0]=='p'){
                sprintf(PT_send_buffer, "%f", 120*fix2float14(current));
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //limit current to be no greater than this, otherwise shut off relay
            else if (cmd[0]=='l'){
                sprintf(PT_send_buffer, "limit %fA", value_f);
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            //get status of device
            else if (cmd[0]=='s'){
                sprintf(PT_send_buffer, "STATUS");
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }
            else if (cmd[0]=='f'){
                if(value_i >= 0 && value_i < N_WAVE/2){
                    sprintf(PT_send_buffer,"%f", fix2float14(fr[value_i]));
                    PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                }
                else{
                    sprintf(PT_send_buffer, "bad freq index");
                    PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                }
            }
            //report raw adc reading after rms calculation
            else if (cmd[0]=='a'){
                //adc_raw
                sprintf(PT_send_buffer, "%d", adc_raw);
                //sprintf(PT_send_buffer, "%f", fix2float16(adc_mean));
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

//=== FFT ==============================================================


void FFTfix(fix14 fr[], fix14 fi[], int m){
//Adapted from code by:
//Tom Roberts 11/8/89 and Malcolm Slaney 12/15/94 malcolm@interval.com
//fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
//size of data = 2**m
// This routine does foward transform only
    int mr,nn,i,j,L,k,istep, n;
    int qr,qi,tr,ti,wr,wi;

    mr = 0;
    n = 1<<m;   //number of points
    nn = n - 1;

    /* decimation in time - re-order data */
    for(m=1; m<=nn; ++m)
    {
        L = n;
        do L >>= 1; while(mr+L > nn);
        mr = (mr & (L-1)) + L;
        if(mr <= m) continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
        //ti = fi[m];   //for real inputs, don't need this
        //fi[m] = fi[mr];
        //fi[mr] = ti;
    }

    L = 1;
    k = LOG2_N_WAVE-1;
    while(L < n){
        istep = L << 1;
        for(m=0; m<L; ++m){
            j = m << k;
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];
            //wr >>= 1; do need if scale table
            //wi >>= 1;
            for(i=m; i<n; i+=istep){
                j = i + L;
                tr = multfix14(wr,fr[j]) - multfix14(wi,fi[j]);
                ti = multfix14(wr,fi[j]) + multfix14(wi,fr[j]);
                qr = fr[i] >> 1;
                qi = fi[i] >> 1;
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            }
        }
        --k;
        L = istep;
    }
}
    
// === FFT Thread =============================================
    
// DMA channel busy flag
#define CHN_BUSY 0x80
static PT_THREAD (protothread_fft(struct pt *pt))
{
    PT_BEGIN(pt);
    static int sample_number ;
    static fix14 zero_point_4 = float2fix14(0.4) ;
    // approx log calc ;
    static int sx, y, ly, temp ;
    static fix16 current_wout_fudge;
    
    while(1) {
        // yield time 0.7 second
        PT_YIELD_TIME_msec(700);
        // enable ADC DMA channel and get
        // 512 samples from ADC
        DmaChnEnable(0);
        // yield until DMA done: while((DCH0CON & Chn_busy) ){};
        PT_WAIT_WHILE(pt, DCH0CON & CHN_BUSY);
        // 
        // profile fft time
        WriteTimer4(0);
        
        // compute and display fft
        // load input array
        for (sample_number=0; sample_number<N_WAVE-1; sample_number++){
            // window the input and perhaps scale it
            fr[sample_number] = multfix14(v_in[sample_number], window[sample_number]); 
            fi[sample_number] = 0 ;
        }
        
        // do FFT
        FFTfix(fr, fi, LOG2_N_WAVE);
        // get magnitude and log
        // The magnitude of the FFT is approximated as: 
        //   |amplitude|=max(|Re|,|Im|)+0.4*min(|Re|,|Im|). 
        // This approximation is accurate within about 4% rms.
        // https://en.wikipedia.org/wiki/Alpha_max_plus_beta_min_algorithm
        for (sample_number = 0; sample_number < N_WAVE/2; sample_number++) {  
            // get the approx magnitude
            fr[sample_number] = abs(fr[sample_number]); //>>9
            fi[sample_number] = abs(fi[sample_number]);
            // reuse fr to hold magnitude
            fr[sample_number] = max(fr[sample_number], fi[sample_number]) + 
                    multfix14(min(fr[sample_number], fi[sample_number]), zero_point_4); 
        }
        //         0.707 * 10 * current sensor output amplitude
        //OLD 
        //current_wout_fudge = CUR_SENSOR_RMS * (fr[SIXTY_HZ] << 6);
        //current = 1.16*current_wout_fudge - 0.043;
        current = 1367*fr[SIXTY_HZ] - 0.318;
        if(current < 0) current = 0;
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // FFT thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
   //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 
  
  // timer 3 setup for adc trigger  ==============================================
  // Set up timer3 on,  no interrupts, internal clock, prescalar 1, compare-value
  // This timer generates the time base for each ADC sample. 
    // works at ??? Hz
    #define sample_rate 1000
    #define prescalar 64
    // 40 MHz PB clock rate
    #define timer_match 40000000/(64*sample_rate)
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_64, timer_match); 
    
    //=== DMA Channel 0 transfer ADC data to array v_in ================================
    // Open DMA Chan1 for later use sending video to TV
    #define DMAchan0 0
    DmaChnOpen(DMAchan0, 0, DMA_OPEN_DEFAULT);
    DmaChnSetTxfer(DMAchan0, (void*)&ADC1BUF0, (void*)v_in, 2, N_WAVE*2, 2); //512 16-bit integers
    DmaChnSetEventControl(DMAchan0, DMA_EV_START_IRQ(28)); // 28 is ADC done
  
  // the ADC ///////////////////////////////////////
  // configure and enable the ADC
  CloseADC10();	// ensure the ADC is off before setting the configuration

  // define setup parameters for OpenADC10
  // Turn module on | ouput in integer | trigger mode auto | enable autosample
  // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
  // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
  // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
  #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_TMR | ADC_AUTO_SAMPLING_ON //

  // define setup parameters for OpenADC10
  // ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
  #define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF

  // Define setup parameters for OpenADC10
  // for a 40 MHz PB clock rate
  // use peripherial bus clock | set sample time | set ADC clock divider
  // ADC_CONV_CLK_Tcy should work at 40 MHz.
  // ADC_SAMPLE_TIME_6 seems to work with a source resistance < 1kohm
  #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_6 | ADC_CONV_CLK_Tcy //ADC_SAMPLE_TIME_5| ADC_CONV_CLK_Tcy2

  // define setup parameters for OpenADC10
  // set AN11 and  as analog inputs
  #define PARAM4	ENABLE_AN11_ANA // pin 24, RB13

  // define setup parameters for OpenADC10
  // do not assign channels to scan
  #define PARAM5	SKIP_SCAN_ALL

  // use ground as neg ref for A | use AN11 for input A     
  // configure to sample AN11  
  SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11); // configure to sample AN411
  OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5); // configure ADC using the parameters defined above

  EnableADC10(); // Enable the ADC
  //end of ADC setup
  
  // timer 4 setup for profiling code ===========================================
  // Set up timer2 on,  interrupts, internal clock, prescalar 1, compare-value
  // This timer generates the time base for each horizontal video line
  OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_8, 0xffff);
        
  // === init FFT data =====================================
  // one cycle sine table
  //  required for FFT
  int ii;
  for (ii = 0; ii < N_WAVE; ii++) {
      Sinewave[ii] = float2fix14(sin(6.283 * ((float) ii) / N_WAVE)*0.5);
      window[ii] = float2fix14(1.0 * (1.0 - cos(6.283 * ((float) ii) / (N_WAVE - 1))));
      //window[ii] = float2fix(1.0) ;
  }
  
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
  PT_INIT(&pt_fft);
  // seed random color
  srand(1);

  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      PT_SCHEDULE(protothread_cmd(&pt_cmd));
      PT_SCHEDULE(protothread_fft(&pt_fft));
    }
  } // main

// === end  ======================================================

