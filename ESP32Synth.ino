/**
 * ESP32Synth
 * 
 * TODO:
 * create exponential scale for adsr knob values (only calculated when knob is rotated, so no lookup tables needed!)
 * implement all missing features like FM, LFO, Reverb and Low/Hi pass
 */


/*
 * INCLUDES
 */
// Nokia 5110 LCD libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Fonts/Picopixel.h>

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

uint32_t waveForm = SAW;          // currently selected waveform
uint32_t pwmWidth = 1023;         // width of square wave (0-2047)


//----------- PINS ---------------
#define DAC_OUT     25            // Audio out using 8 bit internal DAC
// MIDI input is on serial port 0 (RX). This means that USB serial should also work


//--------- INTERRUPT ---------------
#define SAMPLERATE  40000         // samples per second (40KHz)
#define ISRMS       25            // ms delay between each interrupt, should be synced with SAMPLERATE (40KHz)

uint32_t mil        = 0;          // replaces millis() and is calculated during interrupt
uint32_t globalTic  = 0;          // ticks at SAMPLERATE, used for counting to 1ms

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
  uint32_t output;          // output of ADSR, has range of 0-4095
  uint32_t lastOutput;      // previous output value (for release value calculation)
} ADSR;


// ------------ VOICE --------------
#define MAXVOICES   20      // how many voices are allowed to be active at the same time

// Voice data structure
typedef struct {
  ADSR      adsr;           // ADSR values (each voice has its own set of ADSR values)
  uint32_t  wave;           // selected wave (currently unused. only global waveForm variable is used)
  uint32_t  note;           // note ID of voice
  uint32_t  velocity;       // velocity of current note (currently unused, should be relatively easy to implement)
  uint32_t  indexStep;      // by how much we increase the wave table index for each sample (changes with frequency)
  uint32_t  tableIndex;     // current index of wave table
  int32_t   output;         // integer output of voice, has range of +-2047
  uint32_t  activationTime; // millis when the voice was activated
} Voice;

Voice voices[MAXVOICES];    // all voices in one array

//------------- GRAPHICS ---------------
//clk, din, dc, cs, rst
Adafruit_PCD8544 display(0, 4, 2, 15, 13); // using software SPI

#define DISP_HEIGHT 48
#define DISP_WIDTH  84

bool sendFrameBuffer;                 // true if we want to send the framebuffer to the display

// menu pages
#define PAGES       9
#define PAGE_WAVE   0
#define PAGE_ADSR   1
#define PAGE_FM     2
#define PAGE_LFO    3
#define PAGE_REVERB 4
#define PAGE_PWM    5
#define PAGE_PASS   6
#define PAGE_VOICES 7
#define PAGE_SCOPE  8

uint32_t  page = PAGE_WAVE;           // the current menu page

// scope display variables:
uint32_t scopeDelay           = 87;   // delay in frames before sending framebuffer to display (87 seems pretty readable and still fast enough on 5110)
uint32_t scopeYprev           = 0;    // previous y value in scope
uint32_t scopeRefreshCounter  = 0;    // millis of last sent frame
uint32_t scopeX               = 0;    // current x position in scope
uint32_t scopeAmpl            = 0;    // amplitude of audio for scope

int32_t scopeWaveForm[DISP_WIDTH];    // waveform data for display

uint32_t milPrev              = 0;    // millis of last sent framebuffer (for VOICES page)

#define DISP_ADSR_HEIGHT_OFFSET 8     // height offset for ADSR graph, to account for menu name

// ADSR values for display
uint32_t dispAttack;
uint32_t dispDecay;
uint32_t dispSustain;
uint32_t dispRelease;


//------------- MIDI ---------------
#define MIDICONTROLMAXVAL 127 // maximum value of a MIDI control byte

uint8_t mBuffer[3]  = {0,0,0};  // MIDI argument buffer of three bytes
uint8_t mIdx        = 0;        // current index of MIDI buffer
uint8_t cmdLen      = 2;        // number of arguments of current midi command
uint8_t command     = 0;        // midi command type


/*
 * FUNCTIONS
 */

// What to do when a note is pressed
// Looks for an idle voice slot to assign it to
// If it cannot find an idle slot, it will look for a voice slot with the same note ID 
// If it cannot find slot with the same note ID, it will look for the slot with the lowest amplitude in release state
// If a slot is still not found, it will select the oldest slot
//
// Args:
//  - channel:  MIDI channel of pressed note
//  - noteID:   note ID of pressed note
//  - velocity: velocity of pressed note
void handleNoteOn(uint8_t channel, uint8_t noteID, uint8_t velocity) 
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
      if(voices[n].note == noteID)
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
    slot = 0;                                   // to make sure slot will always be a valid value
    uint32_t lowestTime = 0xFFFFFFFF;           // initialize to max uint32_t value
    for (uint32_t n=0; n < MAXVOICES; n++) 
    {
      if (voices[n].activationTime < lowestTime)
      {
        lowestTime = voices[n].activationTime;  // update the lowestTime
        slot = n;                               // update the current found slot
      }
    }
  }

  // when we arrive here, we have found a slot
  voices[slot].note = noteID;         // set the note ID
  voices[slot].activationTime = mil;  // set the activation time
  uint32_t f = getFreq(noteID);       // get freqency of note ID
  setVoiceFreqency(f, slot);          // set the freqency
  setGateOn(slot);                    // notify the ADSR that the note was pressed
}


// What to do when a note is released
// Looks for all voices with the same note ID as the released note ID
// Also checks if the voice is not idle and already in release state, since we want to ignore those
// Then tells the ADSR to release those voices
//
// Args:
//  - channel:  MIDI channel of released note
//  - noteID:   note ID of released note
//  - velocity: velocity of released note
void handleNoteOff(uint8_t channel, uint8_t noteID, uint8_t velocity) 
{
  for (int n=0; n < MAXVOICES; n++) 
  {
    if(voices[n].note == noteID && voices[n].adsr.state != ADSR_IDLE && voices[n].adsr.state != ADSR_RELEASE)
    {
      setGateOff(n);
    }
  }
}


// Returns the current audio sample by doing the following steps:
//  - update the states of the active voices and ADSRs
//  - apply ADSR to the outputs of all active voices
//  - mix those outputs together
//  - scale output to range of DAC
//  - apply clipping where needed
//  - convert signed sample to unsigned sample
//  - return unsigned sample
uint32_t produceSample() 
{
  // update the states of the active voices and ADSRs
  for (uint32_t n = 0; n < MAXVOICES; n++) 
  {
    if ( voices[n].adsr.state != ADSR_IDLE) 
    {
      updateVoice(n);
      updateADSR(n);  
    }
  }

  // apply ADSR to the outputs of all active voices and add them together
  int32_t sum = 0;  
  for (uint32_t n = 0; n < MAXVOICES; n++) 
  {
    if (voices[n].adsr.state != ADSR_IDLE) 
    {
      // voice output is in range of +-2047
      // ADSR output is in range of 0-4095
      // multiply both outputs and divide by 4096 to get an output of +-2047
      int32_t signedADSR = voices[n].adsr.output;   // convert uint32_t to int_32t
      int32_t ADSRappliedOutput = (signedADSR * voices[n].output) >> 12;
      sum = sum + ADSRappliedOutput;             
    } 
  }
       
  // scale output to range of DAC
  sum = sum >> 7; // divide by 8 to allow for 8 peaks at the same time, then divide by 16 to reduce amplitude from +-2047 to +-128.
  
  // apply hard clipping if needed
  if (sum > 127)
    sum = 127;
  if (sum < -128)
    sum = -128;

  // convert signed sample to unsigned sample
  uint32_t unsignedSample = 128 + sum;  // add 128 since the DAC has a range of 0-255

  scopeAmpl = unsignedSample;           // make a copy of the sample for the scope display
  
  return unsignedSample;
}


// Interrupt routine of synthesizer (40KHz)
// Increments counters
// Produces and outputs a sample
void synthInterrupt() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  globalTic++;          // counter at 40KHz
  if (globalTic >= 40)  // 40 tics at 40KHz is 1ms
  {
    globalTic = 0;      // reset counter
    mil++;              // increase millis counter
  }
  
  // generate and output a sample
  dacWrite(DAC_OUT, produceSample());
  
  portEXIT_CRITICAL_ISR(&timerMux);
}


// Things to do at bootup
void setup() 
{
  Serial.begin(115200); // setup serial for MIDI and PC communication
  
  initVoices();
  initADSR();
  initDisplay();

  // setup timer for interrupt
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, synthInterrupt, true);
  timerAlarmWrite(timer, ISRMS, true);
  timerAlarmEnable(timer);
}


// Things to do when not generating sounds
void loop() 
{
  checkMIDI();        // check for MIDI commands
  updateDisplay();    // update the display
}
