/**
 * STM32duino synth
 * 
 * CLEANUP TODO:
 * fix the atan clipping mehtod
 * make unneeded global variables local
 * rename variables and functions
 * improve polyphony checking by looking at note with lowest amplitude
 * use proper data types (int uint32_t int32_t etc.)
 * properly comment code
 * create exponential scale for adsr values (only calculated when knob is rotated, so no lookup tables!!!)
 * create display which shows which notes are active/pressed (polyphony)
 * try to put (at least some) tables in RAM to increase speed
 */


/*
 * INCLUDES
 */
// Nokia 5110 LCD libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Note ID to frequency table
#include "MidiTable.h"  

// ADSR exponential tables (length and max value of 4096)
#include "ExpTable.h"

// Wave tables (amplitude of +-2048)
#include "Sinus2048Int.h"
#include "Saw2048Int.h"
#include "Tri2048Int.h"


/*
 * DEFINITIONS AND VARIABLES
 */

//----------- WAVE TABLES -------------
// length of wavetables (must be a power of 2)
#define WAVETABLELENGTH 2048

// wave selection IDs
#define SQR  0    // square
#define SAW  1    // sawtooth
#define TRI  2    // triangle
#define SIN  3    // sinus
#define FM  4     // FM waves
#define WMAX  5   // number of waves

uint32_t waveForm;                // currently selected waveform
uint32_t pwmWidth = 1023;         // width of square wave (0-2047)


//----------- PINS ---------------
#define DAC_OUT     25            // Audio out using 8 bit internal DAC
// MIDI input is on serial port 0 (RX). This means that USB serial should also work


//--------- INTERRUPT ---------------
#define SAMPLERATE  40000         // samples per second (40KHz)
#define ISRMS       25            // ms delay between each interrupt, should be synced with SAMPLERATE (40KHz)

uint32_t mil        = 0;          // replaces millis() and is calculated during interrupt
uint32_t globalTic  = 0;          // ticks at SAMPLERATE, counts the produced samples

// ESP32 timer for interrupt
hw_timer_t * timer      = NULL;
portMUX_TYPE timerMux   = portMUX_INITIALIZER_UNLOCKED;


//------------ ADSR --------------
// ADSR state codes
#define ADSR_IDLE      0
#define ADSR_ATTACK    1
#define ADSR_DECAY     2
#define ADSR_SUSTAIN   3
#define ADSR_RELEASE   4

// Max time for attack, decay and release in seconds
#define ADSR_MAXATTACKTIME    3
#define ADSR_MAXRELEASETIME   6
#define ADSR_MAXDECAYTIME     8

uint32_t AmaxCount  = 0;    // number of counts within attack state (duration in samples)
uint32_t DmaxCount  = 0;    // number of counts within decay state (duration in samples)
uint32_t RmaxCount  = 0;    // number of counts within release state (duration in samples)
uint32_t Sval       = 4095; // sustain value

// ADSR data structure
typedef struct {
  uint32_t state;           // current ADSR state
  uint32_t counter;         // counter for calculations within current ADSR state
  uint32_t output;          // output of ADSR
  uint32_t lastOutput;      // previous output value (for release value calculation)
} ADSR;


// ------------ VOICE --------------
#define MAXVOICES   12      // how many voices are allowed to be active at the same time

// Voice data structure
typedef struct {
  ADSR      adsr;           // ADSR values (each voice has its own set of ADSR values)
  uint32_t  wave;           // selected wave (currently equal to the globally selected wave
  uint32_t  note;           // note ID of voice
  uint32_t  velocity;       // velocity of current note (currently unused, should be relatively easy to implement)
  uint32_t  indexStep;      // by how much we increase the wave table index for each sample (changes with frequency)
  uint32_t  tableIndex;     // current index of wave table
  int32_t   output;         // integer output of voice
  uint32_t  activationTime; // millis when the voice was activated
} Voice;

Voice voices[MAXVOICES];    // all voices in one array

uint32_t activeVoices = 0;  // number of voices that are not idle


//------------- GRAPHICS ---------------
//clk, din, dc, cs, rst
Adafruit_PCD8544 display(0, 4, 2, 15, 13); // using software SPI

#define DISP_HEIGHT 48
#define DISP_WIDTH  84

bool sendFrameBuffer;                 // true if we want to send the framebuffer to the display

// menu pages
#define PAGES       8
#define PAGE_WAVE   0
#define PAGE_ADSR   1
#define PAGE_FM     2
#define PAGE_LFO    3
#define PAGE_REVERB 4
#define PAGE_PWM    5
#define PAGE_PASS   6
#define PAGE_SCOPE  7

uint32_t  page = PAGE_WAVE;           // the current menu page

// scope display variables:
uint32_t scopeDelay           = 87;   // delay in frames before sending framebuffer to display (87 seems pretty readable and still fast enough on 5110)
uint32_t scopeYprev           = 0;    // previous y value in scope
uint32_t scopeRefreshCounter  = 0;    // millis of last sent frame
uint32_t scopeX               = 0;    // current x position in scope
uint32_t scopeAmpl            = 0;    // amplitude of audio for scope

int32_t scopeWaveForm[DISP_WIDTH];    // waveform data for display

#define DISP_ADSR_HEIGHT_OFFSET 8     // height offset for ADSR graph, to account for menu name

// ADSR values for display
uint32_t dispAttack;
uint32_t dispDecay;
uint32_t dispSustain;
uint32_t dispRelease;


//------------- MIDI ---------------

uint8_t mBuffer[3]  = {0,0,0};  // MIDI argument buffer of three bytes
uint8_t mIdx        = 0;        // current index of MIDI buffer
uint8_t cmdLen      = 2;        // number of arguments of current midi command
uint8_t command     = 0;        // midi command type

#define MIDICONTROLMAXVAL 127 // maximum value of a MIDI control byte


/*
 * FUNCTIONS
 */

// What to do when a note is pressed.
// Looks for an idle voice slot to assign it to.
// If it cannot find an idle slot, it will look for a voice slot with the same note ID 
// If it cannot find slot with the same note ID, it will look for the slot with the lowest amplitude in release state
// If a slot is still not found, it will select the oldest slot
//
// Args:
//  - inChannel:  MIDI channel of pressed note
//  - inNote:     note ID of pressed note
//  - inVelocity  velocity of pressed note
void handleNoteOn(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) 
{
  int32_t slot = -1;  // a slot of -1 means that no slot has been selected
  
  // looks for an idle slot
  for (uint32_t n=0; n < MAXVOICES; n++) 
  {
    if(voices[n].adsr.state == ADSR_IDLE)
    {
      slot = n;
      break;
    }
  }

  // if no slot was found, look for a slot with the same note ID 
  if (slot == -1) 
  {
    for (uint32_t n=0; n < MAXVOICES; n++) 
    {
      if(voices[n].note == inNote)
      {
        slot = n;
        break;
      }
    }
  }

  // if no slot was found, look for a slot with the lowest amplitude in release state
  if (slot == -1) 
  {
    uint32_t lowestRelease = 0xFFFFFFFF; // initialize to max uint32_t value
    for (uint32_t n=0; n < MAXVOICES; n++) 
    {
      // if the voice is in release state
      if(voices[n].adsr.state == ADSR_RELEASE)
      {
        // if the adsr output is lower than the previously found lowest output
        if (voices[n].adsr.output < lowestRelease)
        {
          lowestRelease = voices[n].adsr.output;  // update the lowestRelease
          slot = n;                               // update the current found slot
        }
      }
    }
  }

  // if still no slot was found, select the oldest slot
  if (slot == -1) 
  {
    uint32_t lowestTime = 0xFFFFFFFF; // initialize to max uint32_t value
    for (uint32_t n=0; n < MAXVOICES; n++) 
    {
      if (voices[n].activationTime < lowestTime)
      {
        lowestTime = voices[n].activationTime;  // update the lowestTime
        slot = n;                               // update the current found slot
      }
    }
  }

  voices[slot].note = inNote;         // set the note ID
  voices[slot].activationTime = mil;  // set the activation time
  uint32_t f = getFreq(inNote);       // get freqency of note ID
  setVoiceFreqency(f, slot);          // set the freqency
  setGateOn(slot);                    // notify the ADSR that the note was pressed
}


void handleNoteOff(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity) 
{
  for (int n=0; n < MAXVOICES; n++) 
  {
    if(voices[n].note == inNote)
    {
      setGateOff(n);
    }
  }
}



// -------------------




 /**
 * Here we create the actual sound
 * This method is called 40.000 time per second in the interrupt routine!
 */
void doVoice() {
    // Increment envelopes and oszillators
    for (uint16_t n = 0; n < MAXVOICES; n++) {
        if ( voices[n].adsr.state != ADSR_IDLE) {

         
           addStep(n);                 // Audio value, all voices+
           
          addADSRStep(n);  
        }
    }

      
    //-------------- Audio Mixer -----------------
    int32_t sum = 0;      
    activeVoices = 0;
    for (int16_t n = 0; n < MAXVOICES; n++) {
     if (voices[n].adsr.state != ADSR_IDLE) {
         int32_t v = voices[n].output;   // +-2047 as amplitude
         int32_t env = voices[n].adsr.output; // envelope amplitude, 0-4095
         // --- Envelope VCA: ----
         int32_t act = (env * v) >> 12; //+-2047
         sum = sum + act;         
        
         activeVoices++;   
      } 
    }
       

    
    //----------------- Synth Audio Mixer  --------------------
    if (activeVoices > 0)
    {
      sum = sum >> 7; // divide by 8 to allow for 8 peaks at the same time, then divide by 16 to reduce amplitude from +-2047 to +-128.

      // do hard clipping
      // TODO implement soft clipping
      if (sum > 127)
        sum = 127;
      if (sum < -128)
        sum = -128;
      
          
      // until here, all signals are +- values, so now add half amplitude to get positive values only:
      sum = 128 + sum;
      scopeAmpl = sum;
      
    
      //ledcWrite(PWMCHANNEL, sum);
      dacWrite(DAC_OUT, sum);
      
    }
    else {

      dacWrite(DAC_OUT, 128);
      scopeAmpl = 128;
    }

  
  }


  /**
 * Interrupt routine, triggered at 40Khz 
 * In this routine, we travel along the wavetable and output the amplitude values we find.
 */
 void handler_Synth() {
  portENTER_CRITICAL_ISR(&timerMux);

  globalTic++;            // count up all the time at 40khz
  if (globalTic >= 40) {
    globalTic = 0;        // 40 tics at 40 khz is one millisec.
    mil++;                // increase millisecond tic
    if (mil % 1000 == 0)
    {
      Serial.println(".");
    }
  }
  doVoice();              // the actual sound generation


  
  
  portEXIT_CRITICAL_ISR(&timerMux);
 }

  







void setup() {
  initVoices();
  initDisplay();

  DisplaySetAtValue(8);
  DisplaySetDeValue(10);
  DisplaySetSusValue(4095);
  DisplaySetReValue(13);
  
  sendFrameBuffer = true; // inital draw of the display


  delay(200);
  Serial.begin(115200); // serial for pc communication


  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, handler_Synth, true);
  timerAlarmWrite(timer, ISRMS, true);
  timerAlarmEnable(timer);

  
  delay(200);

}



void loop() {
  readMIDI();
  
  updateDisplay();
   
}
