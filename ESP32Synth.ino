/**
 * STM32duino synth
 * 
 * CLEANUP TODO:
 * fix the atan clipping mehtod
 * remove all unnessecairy features
 * make unneeded global variables local
 * rename variables and functions
 * replace voice with same note if possible to improve polyphony
 * use proper data types (int uint32_t uint8_t etc.)
 * improve adsr value calculation
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#include "Sinus2048Int.h" // 1024
#include "Saw2048Int.h"   // 1024
#include "Tri2048Int.h"   // 1024
#include "MidiTable.h"    
#include "Log.h"          // 4096 // log and antilog table


//----------- Audio clipping algorithm ---------------
// #define USE_ATAN_MIX 
#define USE_CLIP_MIX 

# if defined USE_ATAN_MIX   
  #include "Atan.h"  
#endif   


//----------- Pins ---------------
#define PWM_OUT       25    // Audio out using PWM
// MIDI input is on serial port 0 (RX)



//--------- Interrupt & PWM parameters ---------------
#define PWMRATE     50000    // pwm frequency and is used to traverse wave tables 
#define ISRMS       20      // uS delay between calling handleSynth interrupt (50KHz)
#define PWMCHANNEL  0     // channel for ESP32 PWM
#define PWMRESOLUTION  10    // ESP32 PWM PWMRESOLUTION

//TODO investigate why timing works by counting to 40 at 50KHz
uint32_t mil = 0;               // millisec. Replace millis() and is calculated during interrupt
uint32_t globalTic = 0;         

// timers for ESP32
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


//------------ Vars --------------
// 0=saw, 1=sin,2=tri, 3=sqr, 4=FM
uint32_t sNo = 1;   // selected waveform
uint32_t pwmWidth = 1023; // width of square wave (0-2047)

/*
 * ADSR variables
 */

#define MAXVOICE      12  // amount of polyphony

// Max ADR time in ~seconds
#define ATTACKTIME    3
#define RELEASETIME   6
#define DECAYTIME     8


/**
 * ADSR state codes
 */
#define ATTACK    1
#define DECAY     2
#define SUSTAIN   3
#define RELEASE   4
#define STOP      0
#define STOPPING  100


typedef struct {
  uint32_t A_Step;      // attack step 
  uint32_t D_Step;      // decay step
  uint32_t sustainVal;  // sustain value
  uint32_t R_Step;      // release step
 } ADSR_type;



ADSR_type adsrValues; 

/**
 * Per voice we have a set of adsr values actually running
 */
typedef struct {
  unsigned long ATableIndex;
  uint32_t ADSR_mode;
  uint32_t output;
  uint32_t lastVal; // the last value, when note off came
} ADSR_OfVoice_type;


ADSR_OfVoice_type vAdsr[MAXVOICE+1]; // MAXVOICE is the filter adsr index (last in the array)

uint32_t VirtualTableIndexMax; // here the log-tables overflows




/*
 * GRAPHICS variables
 */

#define DISP_HEIGHT 48
#define DISP_WIDTH  84


bool requestToUpdate = true; // true if we want to send the framebuffer to the display

//clk, din, dc, cs, rst
Adafruit_PCD8544 display(0, 4, 2, 15, 13); // using software SPI


// Vars for scope display:
uint32_t scopeDelay = 87;         // delay in frames before sending framebuffer to display (87 seems pretty readable and still fast enough on 5110)
uint32_t scopeYprev = 0;          // previous y value in scope
uint32_t scopeRefreshCounter = 0; // previous millis time of last sent frame
uint32_t scopeX = 0;              // current x position in scope


uint32_t currentlyPlayingVoices = 0;
uint32_t vres = 0;

int32_t waveFormD[128];   // waveform data for display



#define DISP_ADSR_HEIGHT_OFFSET 8
uint32_t dispAttack, dispDecay, dispSustain, dispRelease; // ADSR values for display


#define PAGES       8
#define PAGE_WAVE   0
#define PAGE_ADSR   1
#define PAGE_FM     2
#define PAGE_LFO    3
#define PAGE_REVERB 4
#define PAGE_PWM    5
#define PAGE_PASS   6
#define PAGE_SCOPE  7

uint32_t  page = PAGE_WAVE;    // the current menu page


/*
 * VOICE variables
 */

#define SAW  0
#define SIN  1
#define TRI  2
#define SQR  3
#define FM  4
#define WMAX  5

#define pwmVolume 1024
#define pwmVolume2 512

#define precisionShift 12
#define TL 11 // table length

// FM vars
int32_t fm_modulator = 256; // 2048
int32_t v_start = 384;  //2048
int32_t v_end = 256;    //2048
int32_t fm_decay = 1024;  //2048



uint32_t fm_tableLength = SinusLengthInt; //2048;
int32_t fm_volume = SinusLengthInt - 1; // +- 2047

uint32_t divider = pow(2, (TL - 8));
uint32_t divider2 = pow(2, (precisionShift - 8));
uint32_t scale = precisionShift - TL ;
int32_t ScaleV =  pow(2, scale);

uint64_t fm_mask = pow(2, TL+precisionShift) - 1;
uint64_t range = fm_tableLength << precisionShift;



/**
 * Note: the voice-index is the same as the adsr index! (a slot)
 */
typedef struct {
  uint32_t note;
  uint32_t step;
  uint32_t modulatorStep;
  uint64_t tableIndex;
  uint64_t modTableIndex;
  int32_t vOutput;
  uint32_t activationTime;
  uint64_t tableIndexEnv3;
  uint32_t envStep3;
  int32_t volDiff;
  int32_t fmVol;
  
 } Voice_type;


/**
 * One entry per simultane playing voice
 */
Voice_type voice[MAXVOICE]; 
 
uint32_t waveForm;              // which wavetable to play (saw, sin etc.)
uint32_t mTableIndexMax; // here the table overflows
uint32_t tableLength; // length of this wavetable




 void handleAllNoteOff() {
   for (int n=0; n < MAXVOICE; n++) {
     if(voice[n].note > 0){
         setGateOff(n);
     }
   }
 }
  
/**
 * Command received from Midi, stop a voice
 */
 void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) 
 {
       int slot = findOldestMidiNote(inNote);
       if (slot < 0) {
        return;
       }
       
       setGateOff(slot);
 }

/**
 * A note on signal was read
 */
void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {


      int slot = -1;
      for (int n=0; n < MAXVOICE; n++) {
         if(voice[n].note == 0){
             slot = n;
             break;
         }
      }
      if (slot < 0) {
         // no voice found to use!
         //Serial.println("On: No empty slot found!");
         int oldestSlot = -1;
         // find the oldest note
         uint32_t maxT = 0xffffffff;
         uint32_t maxRT = 0xffffffff;
         for (int n=0; n < MAXVOICE; n++) {
           if (voice[n].activationTime < maxRT &&  vAdsr[slot].ADSR_mode == RELEASE) {
             maxRT =  voice[n].activationTime;
             slot = n; // oldest slot already in Release state
           }
           else if (voice[n].activationTime < maxT) {
             maxT = voice[n].activationTime;
             oldestSlot = n; // oldest note
            // Serial.print("On: actual oldest ");
             //Serial.println(slot);
           }
         }
         // now use the oldest release slot or the oldest slot
         if (slot < 0)
            slot = oldestSlot;
      } 
      vAdsr[slot].ADSR_mode = STOP;
      voice[slot].activationTime = mil;
      
      
      // now trigger the new voice
    
      uint32_t f = getFreq(inNote); // determine freq. from midi note
      if (waveForm != FM)
          setVoiceFreqency(f, slot, mil); // set the step value for the isr
      else
          setFMFrequencys(f, slot, mil);
      voice[slot].note = inNote;
      setGateOn(slot); // start adsr, gate on setGateOn(channel); // start adsr, gate on


}

// -------------------

/**
 * Find an active playing midi note in the voices array
 * @param note the note to look for
 * @return the voice-index or -1 if not found
 */
int16_t findOldestMidiNote(uint16_t note) { 

  
  int16_t slot = -1;
  uint32_t oldestTime = 0xffffffff; // highest 32 bit number
  for (int16_t n = 0; n < MAXVOICE; n++) {
      if (voice[n].note == note) {
        if (vAdsr[n].ADSR_mode > STOP &&  vAdsr[n].ADSR_mode < RELEASE ) {
          if (voice[n].activationTime < oldestTime) {
            slot = n;
            oldestTime = voice[n].activationTime;
          }
        }
      }
   }

  return slot; 
}


 /**
 * Here we create the actual sound
 * This method is called 40.000 time per second in the interrupt routine!
 */
void doVoice() {
    // Increment envelopes and oszillators
    for (uint16_t n = 0; n < MAXVOICE; n++) {
        if ( vAdsr[n].ADSR_mode != STOP && vAdsr[n].ADSR_mode != STOPPING) {
        
          if (waveForm == FM ) {
           // addFMStep(n);
            int16_t e3 = 0;
            voice[n].tableIndexEnv3 += voice[n].envStep3;
            if (voice[n].tableIndexEnv3 < VirtualTableIndexMax) { 
               int16_t t_idx = (uint16_t) ( voice[n].tableIndexEnv3 >> precisionShift); 
               e3 = getDecay(t_idx); //envelope 1 (0-4095)
               uint16_t envE3 = (e3 * voice[n].volDiff) >> 12;
               voice[n].fmVol = v_end + envE3;
            }
          //--------------------------------------------------
            voice[n].tableIndex += voice[n].step;
            voice[n].tableIndex =  voice[n].tableIndex &  0x7fffff;
            voice[n].modTableIndex += voice[n].modulatorStep;
            voice[n].modTableIndex =  voice[n].modTableIndex &  0x7fffff;
            uint16_t fmidx = voice[n].modTableIndex >> precisionShift; 
            int16_t modSin = getSinInt(fmidx); // modulator freq +- 127
            // modulator envelope:
            int32_t envModSin = divider2 * modSin * voice[n].fmVol; // ScaleV * 
            envModSin = envModSin >> 1;
            envModSin = envModSin % 0x7fffff;
            uint64_t addedPhase = voice[n].tableIndex + envModSin;
            addedPhase = addedPhase & 0x7fffff;
            uint16_t bidx = addedPhase >> precisionShift;
            int16_t v = getSinInt(bidx); // 
            voice[n].vOutput = v;
          }
          else {
            // Classic wavetable:
           // addStep(n);                 // Audio value, all voices+
           voice[n].tableIndex += voice[n].step;     // step through the wavetable, the larger the step value, the higher the frequency
           int c = 0;
            // here access the tables, divide tableIndex by 256
           uint32_t tIdx = voice[n].tableIndex >> 8;      // remember: the tableIndex was multiplied by 256, so here we divide
           tIdx = tIdx & 0x7ff; // 2048
   
           // find out which table to use:
           if(waveForm == 0)
             c = getSawInt(tIdx);
           else if(waveForm == 1)
             c = getSinInt(tIdx);  
           else if(waveForm == 2)
             c = getTriInt(tIdx); 
           else if(waveForm == 3)
            c = (tIdx < pwmWidth)?-2047:2047; 
           voice[n].vOutput = c; //  +- 2047
          }
          addADSRStep(n);     // 0-4096  all adsr ???
        }
    }

      
    //-------------- Audio Mixer -----------------
    int32_t sum = 0;      
    currentlyPlayingVoices = 0;
    for (int16_t n = 0; n < MAXVOICE; n++) {
     if (vAdsr[n].ADSR_mode != STOP) {
         // just examine active voices
         // active voice!
         int16_t v = voice[n].vOutput;   // +-2047 as amplitude
         uint16_t env = vAdsr[n].output; // envelope amplitude, 0-4095
         // --- Envelope VCA: ----
         int16_t act = env * v / LogLength;    // Loglength = 4096, act = [+-2047]
         sum = sum + (act >> 1);  // add only 50%  = +-1024
         
        //Serial.print(" added sum=");
        //Serial.print(sum);
         currentlyPlayingVoices++;   
      } 
    }
       

    
    //----------------- Synth Audio Mixer  --------------------
    if (currentlyPlayingVoices > 0)
    {
      sum = sum >> 6; // divide to adjust range to PWM output
      
     # if defined USE_CLIP_MIX   
      if (sum > pwmVolume2)
        sum = pwmVolume2;
      if (sum < -pwmVolume2)
        sum = -pwmVolume2;
     #endif  
     /* //--- 3. use Atan to do the cliping ---------
      # if defined USE_ATAN_MIX   
        uint16_t tv = 0; 
        sum >>= 1;
         if (sum > 709)
          tv = 1420;
        else 
          tv = getATan(sum);
      //uint16_t res = (tv * 1420) >> 8;
        res = tv;
      #endif
      */
      
          
      // until here, all signals are +- values, so now add half amplitude to get positive values only:
      //vres = graphicSum + 512;
      vres = (sum << 3) + 512;
      sum = pwmVolume2 + sum;
      
    
      //pwmWrite(PWM_OUT, sum); // res
      ledcWrite(PWMCHANNEL, sum);
      
      //digitalWrite(PC13, 0); // turn on, voice is active
    }
    else {

      ledcWrite(PWMCHANNEL, pwmVolume2);
      vres = 512;
    }

      // -------- release notes in end of release state for re-use ------
  /**
   * If a voice is in finished release state mode (STOPPING), just clean it up here.
   */
  for (int n=0; n < MAXVOICE; n++) {
   if (vAdsr[n].ADSR_mode == STOPPING) {
    int nt = voice[n].note;
    voice[n].note = 0;
    vAdsr[n].ADSR_mode = STOP;
   }
  }
  
  }


  /**
 * Interrupt routine, triggered every 20us (50Khz)
 * In this routine, we travel along the wavetable and output the amplitude values we find.
 */
 void handler_Synth() {
  //portENTER_CRITICAL_ISR(&timerMux);

  globalTic++;            // count up all the time at 50khz
  if (globalTic >= 40) {
    globalTic = 0;        // 40 tics at 50 khz is less than one millisec.
    mil++;                // increase millisecond tic
  }
  doVoice();              // the actual sound generation


  
  //portEXIT_CRITICAL_ISR(&timerMux);
 }

  







void setup() {
  sNo = 3; 


  v_start =  256;

  v_end = 64;

  fm_decay = 1023;

  initVoices();
  initDisplay();
  initAdsrVals(); // some values to beginn with. Maybe read from EEprom ?
  selectWave(sNo);

  
  requestToUpdate = true; // inital draw of the display


  delay(200);
  Serial.begin(115200); // serial for pc communication


  ledcSetup(PWMCHANNEL, PWMRATE, PWMRESOLUTION); // configure PWM output
  ledcAttachPin(PWM_OUT, PWMCHANNEL);         // attach the channel PWM pin


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
