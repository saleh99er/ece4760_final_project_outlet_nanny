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

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer, pt_cmd, pt_input, pt_output, pt_DMA_output;

// system 1 second interval tick
int sys_time_seconds ;

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
    
    
    while(1) {
        if(sys_time_seconds % 10 == 0){
            mPORTBToggleBits(BIT_0);
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
    static int toggle;
    static char cmd[16]; 
    static int value_i;
      while(1) {
          PT_YIELD_TIME_msec(100);
            //debug
            //sprintf(PT_send_buffer, "t%d", toggle);
            //toggle = !toggle;
          
            //sprintf(PT_send_buffer,"r%d", toggle);
            // by spawning a print thread
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );             
            
            sscanf(PT_term_buffer, "%s %d", cmd, &value_i);
            if (cmd[0]=='r'){
                if (value_i == 0){
                    mPORTAClearBits(BIT_0);
                }
                else{
                    mPORTASetBits(BIT_0);
                }
                sprintf(PT_send_buffer,"r%d done", value_i);
                //sprintf(PT_send_buffer, "%s", PT_term_buffer); //send what was received
                PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
            }            
            
 
          //spawn a thread to handle terminal input
            // the input thread waits for input
            // -- BUT does NOT block other threads
            // string is returned in "PT_term_buffer"
            sprintf(PT_term_buffer, "");
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );
            
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // end of cmd thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
  
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

