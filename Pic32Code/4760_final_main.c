/*
 * Author: Samad and Saleh
 * Original File:        Test of compiler fixed point
 * Original Author:      Bruce Land
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

////////////////////////////////////
// need for rand function
#include <stdlib.h>
// fixed point types
#include <stdfix.h>
////////////////////////////////////


/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// string buffer
char buffer[60];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_timer ;

// system 1 second interval tick
int sys_time_seconds ;

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
    // set up LED to blink
    mPORTASetBits(BIT_0 );	//Clear bits to ensure light is off.
    mPORTASetPinsDigitalOut(BIT_0 );    //Set port as output
    
    
    
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        sys_time_seconds++ ;
        mPORTAToggleBits(BIT_0);
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 

  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt_timer);
  //PT_INIT(&pt_color);
  //PT_INIT(&pt_anim);



  // seed random color
  srand(1);

  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      //PT_SCHEDULE(protothread_anim(&pt_anim));
      }
  } // main

// === end  ======================================================

