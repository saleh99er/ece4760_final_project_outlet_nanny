/*
 * File:        TFT, keypad, DAC, LED, PORT EXPANDER test
 * Direct Digital Synthesis example 
 * DAC channel A has audio
 * DAC channel B has amplitude envelope
 * Author:      Bruce Land
 * For use with Sean Carroll's Big Board
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2.h"
// yup, the expander
#include "port_expander_brl4.h"

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// need for sine function
#include <math.h>
// The fixed point types
#include <stdfix.h>
////////////////////////////////////

// lock out timer interrupt during spi comm to port expander
// This is necessary if you use the SPI2 channel in an ISR
#define start_spi2_critical_section INTEnable(INT_T2, 0);
#define end_spi2_critical_section INTEnable(INT_T2, 1);

////////////////////////////////////
// some precise, fixed, short delays
// to use for extending pulse durations on the keypad
// if behavior is erratic
#define NOP asm("nop");
// 20 cycles 
#define wait20 NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;NOP;
// 40 cycles
#define wait40 wait20;wait20;
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

////////////////////////////////////
// Audio DAC ISR
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

// audio sample frequency
#define Fs 44000.0
// need this constant for setting DDS frequency
#define two32 4294967296.0 // 2^32 
// sine lookup table for DDS
#define sine_table_size 256
//converts int to char
#define KEY_TO_CHAR(x) ((char)(x+16))
//converts char to int
#define CHAR_TO_INT(x) (((int) x)-16-32-1) // - char offset - record - 1st index cuz array

volatile _Accum sine_table[sine_table_size] ;
// phase accumulator for DDS
volatile unsigned int DDS_phase ;
// phase increment to set the frequency DDS_increment = Fout*two32/Fs
// For A above middle C DDS_increment =  = 42949673 = 440.0*two32/Fs
//#define Fout 440.0
volatile unsigned int DDS_increment;
// waveform amplitude
volatile _Accum max_amplitude=2000;

// waveform amplitude envelope parameters
// rise/fall time envelope 44 kHz samples
volatile unsigned int attack_time=500, decay_time=10000, sustain_time=1000 ; 
//  0<= current_amplitude < 2048
volatile _Accum current_amplitude;
// amplitude change per sample during attack and decay
// no change during sustain
volatile _Accum attack_inc, decay_inc ;

// === FM DDS ===
//phase accumulator for main DDS
volatile unsigned int FM_DDS_phase ;
//phase increment to set the frequency DDS increment = Fout*two32/FS
// For A above middle C DDS_increment = = 42949673 = 440.0*two32/Fs
//#define FM_Fout //3*261.8
volatile unsigned int FM_DDS_increment; 
//waveform amplitude 
// BUT NOTE that FM amp is scaled up by 2^16 in the DDS routine
volatile _Accum FM_max_amplitude=8000, FM_out;

// === FM envelope ===
// waveform amplitude envelope parameters
// rise/fall time amplitude envelope parameters
volatile unsigned int FM_attack_time=200, FM_decay_time=1000, FM_sustain_time=0;
// 0<= FM_current_amplitude < 10000
volatile _Accum FM_current_amplitude;
//amplitude change per sample during attack and decay
//no change during sustain
volatile _Accum FM_attack_inc;
volatile _Accum FM_decay_inc;

//== Timer 2 interrupt handler ===========================================
volatile unsigned int DAC_data_A, DAC_data_B ;// output values
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 4 ; // 10 MHz max speed for port expander!!
volatile int interrupt_time = 0;

// interrupt ticks since beginning of song or note
volatile unsigned int song_time, note_time ;

// Modes
enum audio_mode {FREQ_TEST, //plays sinusoid with no envelope as long as pressed
PLAY, //plays Freq modulated sinusoid with envelope
RECORD, //plays nothing but records button presses for playback
PLAYBACK //plays notes back
};

volatile enum audio_mode mode = FREQ_TEST;

// button states for FSM
enum button_state  {NO_PUSH, MAYBE_PUSH, PUSHED, MAYBE_NO_PUSH};

// Recording info
int record_index = 0;
int play_index = 0;
int record_button = 0;
int debug = 0;

//frequency based on key index
int freq[9] = {262, 294, 330, 349, 392, 440, 494, 523, 0};

void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    int junk;
    mT2ClearIntFlag();
    interrupt_time = ReadTimer2();
    // generate  waveform
    // advance the FM phase
    if(mode != FREQ_TEST){ //apply freq modulation
        FM_DDS_phase += FM_DDS_increment ;
        // FM sine wave
        FM_out = (FM_current_amplitude*sine_table[FM_DDS_phase>>24]) ;
        // advance the main phase
        DDS_phase += DDS_increment + ((int)FM_out << 16) ;
        
        // update amplitude envelope 
        if (note_time < (FM_attack_time + FM_decay_time + FM_sustain_time)){
            FM_current_amplitude = (note_time <= FM_attack_time)? 
            FM_current_amplitude + FM_attack_inc : 
            (note_time <= FM_attack_time + FM_sustain_time)? FM_current_amplitude:
                FM_current_amplitude - FM_decay_inc ;
        }
        else {
            FM_current_amplitude = 0 ;
        }
    }
    else{
        DDS_phase += DDS_increment;
    }
    
    DAC_data_A = (int)(current_amplitude*sine_table[DDS_phase>>24]) + 2048 ; // for testing sine_table[DDS_phase>>24]
    
    if (mode != FREQ_TEST){
        // update amplitude envelope 
        if (note_time < (attack_time + decay_time + sustain_time)){
            current_amplitude = (note_time <= attack_time)? 
            current_amplitude + attack_inc : 
            (note_time <= attack_time + sustain_time)? current_amplitude:
                current_amplitude - decay_inc ;
        }
        else {
            current_amplitude = 0 ;
        }
    }
    DAC_data_B = (int) current_amplitude  ;
    
     // test for ready
     while (TxBufFullSPI2());
     
    // reset spi mode to avoid conflict with expander
    SPI_Mode16();
    // DAC-A CS low to start transaction
    mPORTBClearBits(BIT_4); // start transaction 
     // write to spi2
    WriteSPI2(DAC_config_chan_A | (DAC_data_A & 0xfff) );
    // fold a couple of timer updates into the transmit time
    song_time++ ;
    note_time++ ;
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
    // MUST read to clear buffer for port expander elsewhere in code
    junk = ReadSPI2(); 
    // CS high
    mPORTBSetBits(BIT_4); // end transaction
    
     // DAC-B CS low to start transaction
    mPORTBClearBits(BIT_4); // start transaction 
     // write to spi2
    WriteSPI2(DAC_config_chan_B | (DAC_data_B & 0xfff) );
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
    // MUST read to clear buffer for port expander elsewhere in code
    junk = ReadSPI2(); 
    // CS high
    mPORTBSetBits(BIT_4); // end transaction
   // Record timer value to determine interrupt duration
    interrupt_time = ReadTimer2() - interrupt_time + 100;
}

// === print a line on TFT =====================================================
// print a line on the TFT
// string buffer
char buffer[60];
char note_buffer[60];

void printLine(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 10 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(1);
    tft_writeString(print_buffer);
}

void printLine2(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 20 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}

// Predefined colors definitions (from tft_master.h)
//#define	ILI9340_BLACK   0x0000
//#define	ILI9340_BLUE    0x001F
//#define	ILI9340_RED     0xF800
//#define	ILI9340_GREEN   0x07E0
//#define ILI9340_CYAN    0x07FF
//#define ILI9340_MAGENTA 0xF81F
//#define ILI9340_YELLOW  0xFFE0
//#define ILI9340_WHITE   0xFFFF

// === thread structures ============================================
// thread control structs
static struct pt pt_timer, pt_key, pt_play ;

// system 1 second interval tick
int sys_time_seconds ;

// === Timer Thread =================================================
// update a 1 second tick counter
static PT_THREAD (protothread_timer(struct pt *pt))
{
    PT_BEGIN(pt);
     // timer readout
     sprintf(buffer,"%s", "Time in sec since boot\n");
     printLine(0, buffer, ILI9340_WHITE, ILI9340_BLACK);
     
     // set up LED to blink
     mPORTASetBits(BIT_0 );	//Clear bits to ensure light is off.
     mPORTASetPinsDigitalOut(BIT_0 );    //Set port as output
     
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        sys_time_seconds++ ;
        // toggle the LED on the big board
        mPORTAToggleBits(BIT_0);
        
        // draw sys_time
        sprintf(buffer,"%d", sys_time_seconds);
        printLine(1, buffer, ILI9340_YELLOW, ILI9340_BLACK);
	
	// draw sampled interrupt cycles
	sprintf(buffer,"%s", "Cycles for interrupt service");
	printLine(30, buffer, ILI9340_WHITE, ILI9340_BLACK);
	sprintf(buffer, "%d", interrupt_time);
        printLine(31, buffer, ILI9340_YELLOW, ILI9340_BLACK);
	
	//debug, display current audio mode
	sprintf(buffer,"%s", "Mode:");
	printLine(27, buffer, ILI9340_WHITE, ILI9340_BLACK);
	sprintf(buffer,"%s", 
                mode==PLAY ? "PLAY" : 
                mode==FREQ_TEST ? "FREQ_TEST" : 
                mode==RECORD ? "RECORD" :
                mode==PLAYBACK ? "PLAYBACK" :
                    "?");
	printLine(28, buffer, ILI9340_YELLOW, ILI9340_BLACK);
        sprintf(buffer, "debug : %d", debug);
        printLine(29, buffer, ILI9340_YELLOW, ILI9340_BLACK);
        
	//debug, display registered notes
        sprintf(buffer,"%s", "Recorded notes:");
        printLine(3, buffer, ILI9340_WHITE, ILI9340_BLACK);
        printLine(4, note_buffer, ILI9340_BLUE, ILI9340_BLACK);
        sprintf(buffer,"Recording index: %d", record_index);
        printLine(5, buffer, ILI9340_WHITE, ILI9340_BLACK);
        sprintf(buffer,"%d", "Record button: %d", record_button);
        // start a new sound
        // critical section?
        
	
        // !!!! NEVER exit while !!!!
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Keypad Thread =============================================
// Port Expander connections:
// y0 -- row 1 -- thru 300 ohm resistor -- avoid short when two buttons pushed
// y1 -- row 2 -- thru 300 ohm resistor
// y2 -- row 3 -- thru 300 ohm resistor
// y3 -- row 4 -- thru 300 ohm resistor
// y4 -- col 1 -- internal pullup resistor -- avoid open circuit input when no button pushed
// y5 -- col 2 -- internal pullup resistor
// y6 -- col 3 -- internal pullup resistor
// y7 -- shift key connection -- internal pullup resistor
// z7 -- record key connection -- internal pullup resistor

static PT_THREAD (protothread_key(struct pt *pt))
{
    PT_BEGIN(pt);
    static int keypad, i, pattern;
    // order is 0 thru 9 then * ==10 and # ==11
    // no press = -1
    // table is decoded to natural digit order (except for * and #)
    // with shift key codes for each key
    // keys 0-9 return the digit number
    // keys 10 and 11 are * adn # respectively
    // Keys 12 to 21 are the shifted digits
    // keys 22 and 23 are shifted * and # respectively
    // Keys 32 to 41 are a recorded keys 0-9 respectively
    // Keys 42 to 55 are don't cares (recorded buttons combined with some invalid input, ie #)
    // Keys 56 is the record button pushed with no other keys 
    static int keytable[24]=
    //        0     1      2    3     4     5     6      7    8     9    10-*  11-#
            {0xd7, 0xbe, 0xde, 0xee, 0xbd, 0xdd, 0xed, 0xbb, 0xdb, 0xeb, 0xb7, 0xe7,
    //        s0     s1    s2  s3    s4    s5    s6     s7   s8    s9    s10-* s11-#
             0x57, 0x3e, 0x5e, 0x6e, 0x3d, 0x5d, 0x6d, 0x3b, 0x5b, 0x6b, 0x37, 0x67};
    // bit pattern for each row of the keypad scan -- active LOW
    // bit zero low is first entry
    static char out_table[4] = {0b1110, 0b1101, 0b1011, 0b0111};
    
    // init the port expander
    start_spi2_critical_section;
    initPE();
    // PortY on Expander ports as digital outputs
    mPortYSetPinsOut(BIT_0 | BIT_1 | BIT_2 | BIT_3);    //Set port as output
    // PortY as inputs
    // note that bit 7 will be shift key input, 
    // separate from keypad
    mPortYSetPinsIn(BIT_4 | BIT_5 | BIT_6 | BIT_7);    //Set port as input
    mPortZSetPinsIn(BIT_7);
    mPortYEnablePullUp(BIT_4 | BIT_5 | BIT_6 | BIT_7);
    mPortZEnablePullUp(BIT_7);
    
    end_spi2_critical_section ;
    
    // the read-pattern if no button is pulled down by an output
    #define no_button (0x70)
    // mapping of i if it's recording, AND button value with this to determine if recording
    #define is_recording (0x10)
    // mapping of i if only record button is pressed, compare (eq)  button value with this to determine if only record button is pressed
    #define record_button_only (56)

    static int last_pressed; 
    static int last_pressed_debouncing;
    static enum button_state button_state = NO_PUSH;
    static int debounced_button_press = -1;
    
      while(1) {
        // yield time
        PT_YIELD_TIME_msec(30);
    
        //sending pulses to keypad to detect button presses on keypad
        for (i=0; i<4; i++) {
            start_spi2_critical_section;
            // scan each rwo active-low
            writePE(GPIOY, out_table[i]); //bitwise [ 1110, 1101, 1011, 0111]
            //reading the port also reads the outputs
            keypad  = readPE(GPIOY);
            end_spi2_critical_section;
            // was there a keypress?
            if((keypad & no_button) != no_button) { break;}
        }
	//read record button from port Z
	start_spi2_critical_section;
        record_button = readBits(GPIOZ, BIT_7) ^ (BIT_7); //high if pressed
	end_spi2_critical_section;
	
        // search for keycode
        if (keypad > 0 || record_button){ // then button is pushed
            for (i=0; i<24; i++){
                if (keytable[i]==keypad) {
                    if(record_button) i+= (BIT_5); //set 5th bit if recording 
                    break;
                }
            }
            // if invalid, two button push, set to -1
            if (i==24) i=-1;
        }
        else i = -1; // no button pushed

        // draw key number (not debounced)
        if ((i>-1 && i<10) || (i>31 && i<52)) sprintf(buffer,"   %x %d", keypad, i);
        if (i==10 ) sprintf(buffer,"   %x *", keypad);
        if (i==11 ) sprintf(buffer,"   %x #", keypad);
        if (i>11 && i<22 ) sprintf(buffer, "   %x shift-%d", keypad, i-12);
        if (i==22 ) sprintf(buffer,"   %x ahift-*", keypad);
        if (i==23 ) sprintf(buffer,"   %x shift-#", keypad);
        if (i == 56) sprintf(buffer,"%s", "   recording...");
        if (i>-1 && i<12) printLine2(10, buffer, ILI9340_GREEN, ILI9340_BLACK);
        if (i>31 && i<52) printLine2(10, buffer, ILI9340_YELLOW, ILI9340_BLACK);
        if (i>11 && i<22) printLine2(10, buffer, ILI9340_RED, ILI9340_BLACK);
        if (i<-1) printLine2(10, buffer, ILI9340_RED, ILI9340_BLACK);
	
	switch (button_state) {
            case NO_PUSH: 
               if (i != -1) {
                    button_state=MAYBE_PUSH;
                    last_pressed_debouncing = i;
               }
                else {
                   button_state=NO_PUSH;
                   debounced_button_press = -1;
                }
            break;
            case MAYBE_PUSH:
               if (i == last_pressed_debouncing) {
                    button_state=PUSHED;
                    debounced_button_press = i;
                    //recording
                    if((debounced_button_press > 31) && (debounced_button_press < 41)){
                        
                        mode = RECORD;
                        
                        DDS_increment = debounced_button_press == 32 ? 0 : freq[debounced_button_press-33]*two32/Fs ;
                        note_time = 0;
                        current_amplitude = 0;
                        //append key (without record bit) to note_buffer
                        if( !(debounced_button_press == record_button_only) ){
                            note_buffer[record_index] = KEY_TO_CHAR(debounced_button_press);
                            record_index++;
                            note_buffer[record_index] = (char)0; // end of text 
                        }       
                    }
                    else if (debounced_button_press == 10) { //resetting with star key??
                        record_index = 0;
                    }
               }
               else button_state=NO_PUSH;
               break;
            case PUSHED:  
               if (i == last_pressed_debouncing) button_state=PUSHED; 
               else button_state=MAYBE_NO_PUSH;    
               break;
            case MAYBE_NO_PUSH:
               if (i == last_pressed_debouncing) button_state=PUSHED; 
               else button_state=NO_PUSH;    
               break;
	} // end case 
        
	
        if (i == record_button_only){
            mode = RECORD;
        }
        //&& last_pressed != debounced_button_press
        else if (debounced_button_press == 11  && button_state == PUSHED){
            mode = PLAYBACK;
            PT_SPAWN(pt, &pt_play, playback(&pt_play));
            mode = PLAY;
            last_pressed = debounced_button_press;
        }
	else if (debounced_button_press > 0 && debounced_button_press < 9 && (button_state == PUSHED) && last_pressed != debounced_button_press) {
            mode = PLAY;
            DDS_increment = freq[i-1]*two32/Fs ; //42949673 ;
            FM_DDS_increment= 3*DDS_increment*two32/Fs; //3*DDS_increment*two32/Fs;
            note_time=0;
            current_amplitude = 0 ;
            FM_current_amplitude = 0;
            last_pressed = debounced_button_press;
	}	
	if (i > 12 && i < 21){ 
            mode = FREQ_TEST;
            DDS_increment = freq[i-13]*two32/Fs ; //42949673 ;
            current_amplitude = max_amplitude ;
            last_pressed =i;   	
	}
	
	if (i==-1 && mode==FREQ_TEST){
            last_pressed=-1;
            current_amplitude = 0;
	}
        if (i==-1){
            last_pressed=-1;
        }
    
        // !!!! NEVER exit while !!!!
      } // END WHILE(1)
  PT_END(pt);
} // keypad thread

//Spawned Play thread (spawned by keypad thread when # is pressed)
int playback(struct pt *pt){
    PT_BEGIN(pt);
    debug = -1; 
    play_index = 0;
    while(record_index > 0 && play_index < record_index){
        debug = note_buffer[play_index];
        //debug = CHAR_TO_INT(note_buffer[record_index]);
        DDS_increment = freq[CHAR_TO_INT(note_buffer[play_index])]*two32/Fs;
        current_amplitude = 0;
        note_time = 0;
        play_index++;
        PT_YIELD_TIME_msec(500);
    }
    PT_EXIT(pt);
    PT_END(pt);
} //play thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 
  
  //init note buffer
  //note_buffer[0]= (char)2; //start of text
  //note_buffer[1] = (char)3; //end of text

  // set up DAC on big board
  // timer interrupt //////////////////////////
    // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
    // at 40 MHz PB clock 
    // 40,000,000/Fs = 909 : since timer is zero-based, set to 908
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 908);

    // set up the timer interrupt with a priority of 2
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    mT2ClearIntFlag(); // and clear the interrupt flag

    // SCK2 is pin 26 
    // SDO2 (MOSI) is in PPS output group 2, could be connected to RB5 which is pin 14
    PPSOutput(2, RPB5, SDO2);

    // control CS for DAC
    mPORTBSetPinsDigitalOut(BIT_4);
    mPORTBSetBits(BIT_4);

    // divide Fpb by 2, configure the I/O ports. Not using SS in this example
    // 16 bit transfer CKP=1 CKE=1
    // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
    // For any given peripherial, you will need to match these
    // clk divider set to 4 for 10 MHz
    SpiChnOpen(SPI_CHANNEL2, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , 4);
   // end DAC setup
    
   // 
   // build the sine lookup table
   // scaled to produce values between 0 and 4096
   int i;
   for (i = 0; i < sine_table_size; i++){
         sine_table[i] = (_Accum)(sin((float)i*6.283/(float)sine_table_size));
    }
   
   // build the amplitude envelope parameters
   // bow parameters range check
	if (attack_time < 1) attack_time = 1;
	if (decay_time < 1) decay_time = 1;
	if (sustain_time < 1) sustain_time = 1;
	// set up increments for calculating bow envelope
	attack_inc = max_amplitude/(_Accum)attack_time ;
	decay_inc = max_amplitude/(_Accum)decay_time ;
    FM_attack_inc = FM_max_amplitude/(_Accum)FM_attack_time ;
	FM_decay_inc = FM_max_amplitude/(_Accum)FM_decay_time ;
    
  // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();
  
  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // init the threads
  PT_INIT(&pt_timer);
  PT_INIT(&pt_key);
  PT_INIT(&pt_play);
  
  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_timer(&pt_timer));
      PT_SCHEDULE(protothread_key(&pt_key));
      }
  } // main

// === end  ======================================================

