/**
 * STM32duino synth
 * 
 * CLEANUP TODO:
 * remove all channels. keep it simple. will only need one channel anyways (my usb keyboard)
 * rename niksMidi to midi
 * fix the atan clipping mehtod
 * remove all unnessecairy features
 * place global variables where they belong
 * make unneeded global variables local
 * rename variables and functions
 * remove external adsr thingy and input?
 * graphics adsr functions should get variables from global adsr vars
 * replace voice with same note if possible to improve polyphony
 * make seperate file with global variables
 * improve adsr graph horizontal position and such. also drawing speed
 * fix pwm (square wave) second half
 * increase max decay and release length
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#include "Sinus2048Int.h" // 1024
#include "Saw2048Int.h"   // 1024
#include "Tri2048Int.h"   // 1024
#include "MidiTable.h" 
#include "Log.h"   // 4096 // log and antilog table


//--- !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ---------------
#define USE_BOOTLOADER   // uncomment this, when using the stm32 bootloader, or else no Midi is possible

// #define USE_ATAN_MIX   //  Audio mixer methods, use just one!
#define USE_CLIP_MIX   //    

# if defined USE_ATAN_MIX   
  #include "Atan.h"  
#endif   
//----------- Pins ---------------
#define PWM_OUT       25    // PA8 PWM output pin, Audio out
#define ONBOARDLED    0   //  
#define scalePin      0 //PA2    // freq tuning

int ypos = 18;
int zero = 0;

//---------Some debug values ----------------------


const bool doReverbF = false;      // if true, actually use the build in reverb filter 
const bool doFilterADSR = false;    // if true, also generate the filter ad signal for external analog filter
const bool doADSR = true;          // if true, apply an ADSR to each midi note
const bool doScope = true;         // if true, show the actual playing waveform on the display 
//
const bool debugVoice = false;      // if true, the loop will calculate the voice engine. (Used for debugging)
const bool testRev = false;        // If true, just do a test of the reverb filter. (Used onyl for debugging)
//---



 const uint32_t ISR_RATE = 25000;  // interrupt rate 50000Hz

const int isrms = 20;
const int freq = 50000;
const int ledChannel = 0;
const int resolution = 10; //Resolution 8, 10, 12, 15

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;



//MIDI_CREATE_DEFAULT_INSTANCE();
//using namespace midi;
//MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI); 

const boolean LISTENTOALLCHANNELS = false;

//------------ Vars --------------
uint32_t mil = 0;               // millisec. tic, this replace the call: millis()
uint32_t globalTic = 0;           
//uint32_t potDebounce = 0;
int waveFormD[128];

bool start = true;                  // true, while we are in  start-mode
uint16_t testrun = 0;
bool testRuns = false;

String tmp = "";
uint32_t avgsumv = 0;     // experimantal lp filter on synth out, not used!
uint16_t mcountv = 0;
int16_t AVERAGEV = 64;

const int EEP_CHANNEL = 0;
const int WAVE_CHANNEL = 1;
const int DATTACK_CHANNEL = 2;
const int DDECAY_CHANNEL = 3;
const int PWM_CHANNEL = 4;
const int VSTART = 5;
const int VEND = 6;
const int FM_DECAY = 7;

int midiChannel = 1;        // Midi channel


/**
 * Debug voice
 */
bool startDebugVoice = debugVoice;
bool voiceDebugging = false;

// Vars for oszilloscope display:
int16_t vres = 0;
uint16_t dispIdx = 0, refreh = 0, anz = 0;
uint16_t tbase = 1; //
uint16_t tbaseidx = 0;
uint16_t DisplayScale = 6000;

//Analog input
uint16_t aChannel = 0;
uint16_t aDelayIDX = 0;
const uint16_t ADELAY = 50;

bool tstate = false;


byte sNo = 1;
//byte channel = EEPROM.read(WAVE_CHANNEL);        // Midi channel







/*
 * ADSR variables
 */

const uint16_t MAXVOICE = 8;      // 5 

const uint16_t ATTACKTIME = 2;  //  sec.
const uint16_t RELEASETIME = 4; //  sec.
const uint16_t DECAYTIME = 2;   //  sec.

const uint16_t VOICES = 0;
const uint16_t FILTER = 1;

/**
 * ADSR state codes
 */
const uint16_t ATTACK = 1;
const uint16_t DECAY = 2;
const uint16_t SUSTAIN = 3;
const uint16_t RELEASE = 4;
const uint16_t STOP = 0;
const uint16_t STOPPING = 100; 

// only 2 sets of adsr value, one for the VOICE, one for the FILTER
typedef struct {
  uint16_t A_Step;      // attack step 
  uint16_t D_Step;      // decay step
  uint16_t sustainVal;  // sustain value
  uint16_t R_Step;      // release step
 } ADSR_type;


/**
 * 2 sets of adsr values 
 */
ADSR_type adsrValues[2]; 

/**
 * Per voice we have a set of adsr values actually running
 */
typedef struct {
  unsigned long ATableIndex;
  uint32_t ADSR_mode;
  uint16_t output;
  uint16_t lastVal; // the last value, when note off came
} ADSR_OfVoice_type;

uint16_t filter = MAXVOICE;
ADSR_OfVoice_type vAdsr[MAXVOICE+1]; // MAXVOICE is the filter adsr index (last in the array)

unsigned long VirtualTableIndexMax; // here the log-tables overflows



/*
 * CONTROLS variables
 */

//const int SW   = PA15;  // Der Switch wird mit Pin D2 Verbunden. ACHTUNG : ggf. einen interrupt-Pin nehmen!

const uint16_t MAXPOT  = 7;
const uint16_t P_ATTACK  = 0;
const uint16_t P_DECAY  = 1;
const uint16_t P_SUSTAIN  = 2;
const uint16_t P_RELEASE  = 3;
const uint16_t P_REVDELAY  = 4;
const uint16_t P_REVAMOUNT  = 5;
//const uint16_t P_PWM  = 6;
const uint16_t P_MODUL  = 6;

const int AVERAGE =       16;     // For the gliding average method (LP filter for noisy analog values)

uint16_t pin[MAXPOT];             // Analog input pins
uint16_t mcount[MAXPOT]; // 
uint32_t avgsum[MAXPOT]; 
uint16_t Avalue[MAXPOT];  // 0-4096, analog in

// Encoder vars:
uint32_t TimeOfLastDebounce = 0;
uint16_t DelayofDebounce = 10;
int16_t encoderPosCount = 0; 
uint16_t pinALast;  
uint16_t aVal;
boolean bCW;

//Switches & buttons
const int noOfButtons = 4;
const uint16_t ENCODERBT  = 0;
const uint16_t SHIFT  = 1;
const uint16_t SW1  = 2;
const uint16_t SW2  = 3;

typedef struct {
  int portPin;                 // Pin Nunber to read;
  uint16_t debounceDelay;      // debounceDelay = 50;    // the debounce time; increase if the output flickers
  uint16_t lastDebounceTime;   // lastDebounceTime = 0;  // the last time the output pin was toggled
  uint16_t lastButtonState;    // actual state
 } Button_type;
Button_type buttons[noOfButtons];


/*
 * GRAPHICS variables
 */

 
const int width =  84; // pixel-width of display
const int height = 48; // pixel height of display
const int LF = 4;     // line feed size

const int BASE = 0;
const int WAVESEL = 1;
const int DATTACK = 2;
const int DDECAY = 3;
const int SCOPE = 4;
const int DPWM = 5;
const int FMSTART = 6;
const int FMEND = 7;
const int FMDECAY = 8;
const int ADSRP = 9;
//const int MIDISNIF = 7; // not used
const int PAGEMAX = 9;

const int chDispY = LF;
const int noOnY = LF*2;   // note on-off y pos
const int chDispX = 0;   // midi Channel
const int ntOnX = 42;   // note on x pos
const int ntOffX = 85;   // note off x pos
const int curveOffX = 42;
const int ntW = 42; // width of fields

const int ARROW_WIDTH = 8;
const int DGAP = 3;
int WAVEDISP_WIDTH = width - 2*ARROW_WIDTH - 2 * DGAP;
int xstart = ARROW_WIDTH + DGAP;

uint16_t  page = BASE;    // this is the ui-page we actually display
uint16_t  opx = 0;
uint16_t  opy = 0;
uint16_t at, de, sus, rel; // adsr values
int16_t fat, fde, pw; // filter ad values

bool requestToUpdate = true; // if true, execute the  display.display() function
int cx = 0, cy = 0;

//Adafruit_SH1106 display(OLED_SDA, OLED_SCL);
//clk, din, dc, cs, rst
Adafruit_PCD8544 display(0, 4, 2, 15, 13); // using software SPI



/*
 * VOICE variables
 */

const int SAW = 0;
const int SIN = 1;
const int TRI = 2;
const int SQR = 3;
const int FM = 4;
const int WMAX = 5;

const int pwmVolume = 1024;
int pwmVolume2 = pwmVolume / 2;

const int precisionShift = 12;
const int TL = 11; // table length

// FM vars
int fm_modulator = 256; // 
int v_start = 384;      // to eeprom!
int v_end = 256;
int fm_decay = 1024;
uint16_t fm_tableLength = SinusLengthInt; //2048;
int fm_volume = SinusLengthInt - 1; // +- 2047
uint64_t fm_mask = pow(2, TL+precisionShift) - 1;
uint16_t divider = pow(2, (TL - 8));
uint16_t divider2 = pow(2, (precisionShift - 8));
uint16_t scale = precisionShift - TL ;
int16_t ScaleV =  pow(2, scale);
uint64_t range = fm_tableLength << precisionShift;

/**
 * Note: the voice-index is the same as the adsr index! (a slot)
 */
typedef struct {
  uint16_t note;
  uint32_t step;
  uint32_t modulatorStep;
  uint64_t tableIndex;
  uint64_t modTableIndex;
  int16_t vOutput;
  uint32_t activationTime;
  uint64_t tableIndexEnv3;
  uint16_t envStep3;
  int16_t volDiff;
  int16_t fmVol;
  
 } Voice_type;


/**
 * One entry per simultane playing voice
 */
Voice_type voice[MAXVOICE]; 
 
uint16_t waveForm;              // which wavetable to play (saw, sin etc.)
uint32_t mTableIndexMax; // here the table overflows
uint32_t tableLength; // length of this wavetable

// global vars 
int16_t pwm_value = 512;           // 0-1024 (poti value);
int16_t pwm_CValue = 512;           // 0-1024 cv value for pwm;


/*
 * Reverb variables
 */

// reverb filter, using 10kByte Ram = 250ms@40k ISR Rate 
const uint16_t RevLength = 10000;
uint16_t maxR = RevLength -1;

uint16_t delayTime = 0;// how many entrys behind head we read the cache
int16_t writePointer;  // where to write
int16_t readPointer;   // where to read
int16_t diff = 0;      // is positive, when a longer delay time is set. This prevents n on-sensical data to be read.
bool changes = false;

// Analog buffer
int8_t rev[RevLength];  // cache (only 8 bit range)




void readAnalogChannel(int n) {
  if (start)
    return; // not in start mode...

    /*
   bool change = updateAnalogChannel(n);
   if (change) { 
      if (n == P_ATTACK){
        setAttackScale(Avalue[n], VOICES);
        setAtValue(Avalue[n]); // for the graphic 
       // Serial.print("Attack=");
       // Serial.println(Avalue[n]);
      }
      else if (n == P_DECAY){
        setDecayScale(Avalue[n], VOICES);
        setDeValue(Avalue[n]);
      }
      else if (n == P_SUSTAIN){
        setSustainValue(Avalue[n], VOICES);
        setSusValue(Avalue[n]);
      }
      else if (n == P_RELEASE){
        //Serial.print("Release=");
       // Serial.println(Avalue[n]);
        setReleaseScale(Avalue[n], VOICES);
        setReValue(Avalue[n]);
      }
      else if (n == P_REVDELAY){
        setDelayTime(Avalue[n]); // 0-250ms
      }
       else if (n == P_MODUL){
        setFMModulator(Avalue[n]); // 0-2048
      }
      
    }
    */
}

//b1: encoder +
//b2: shift

void actOnEncoder(uint32_t mil) {

}
/**
 * Init ADSR default values to default values, maybe from eeprom?
 */
void setAdsrVals() {
  //Serial.println("Set ADSR values");
    setDelayTime(4000); // 
    Avalue[P_REVAMOUNT] = 4000;
    setAttackScale(20, VOICES);
    setAtValue(20);
    setDecayScale(600, VOICES);
    setDeValue(600);
    setSustainValue(2048, VOICES);
    setSusValue(2048);
    setReleaseScale(200, VOICES);
    setReValue(200);

    // AD Filter setting
    setAttackScale(fat, FILTER);
    setFAtValue(fat);
    setDecayScale(fde, FILTER);
    setFDeValue(fde);
    /*setAttackScale(400, FILTER);
    setFAtValue(400);
    setDecayScale(600, FILTER);
    setFDeValue(600);
    */
    setSustainValue(0, FILTER); // not used
    setReleaseScale(100, FILTER);// not used
}

/**
 * Set reverb amount mix value
 * @param a value of the mix pot [0-4095]
 */
void setRevAmount(uint16_t a) {
  // To be done
}

//------------------------------------------------------------------------------------------






/**
 * Just for debugging, not used in synth!
 */
 void doReverb() {
  // Reverb: ---------- make echo switchable? Or just set 0
    if (delayTime > 0 && diff == 0) {
      uint16_t rv = readRev(); // echo value
      Serial.print("readPointer=");
      Serial.print(readPointer);
      Serial.print(" v=");
      Serial.print(rv);
      Serial.print(" writePointer=");
      Serial.println(writePointer);
    }
    else if (diff > 0) {
      Serial.print("Buffering diff=");
      Serial.print(diff);
      Serial.print(" writePointer=");
      Serial.println(writePointer);
    }
      
    writeHead(1); // write empty 
 }

//-------------------------------

 void handleAllNoteOff() {
   for (int n=0; n < MAXVOICE; n++) {
     if(voice[n].note > 0){
         setGateOff(n); // change adsr state
     }
   }
 }
  
/**
 * Command received from Midi, stop a voice
 * velocity ignored for the moment...
 */
 void handleNoteOff(byte inChannel, byte inNote, byte inVelocity) {
  //dispNoteOff(inNote, 0);
  if (midiChannel == inChannel || LISTENTOALLCHANNELS) { // all notes
     // Midi Off
       int slot = findOldestMidiNote(inNote);
       if (slot < 0) {
       // Serial.print("Error for note ");
       // Serial.println(inNote);
        return;
       }
       
       setGateOff(slot); // change adsr state
       /*if (!doScope) {
          dispNoteOff(inNote, slot);
       }
       */
    }
    
 }

/**
 * A note on signal was read either from MIDI or from the tune-player
 */
void handleNoteOn(byte inChannel, byte inNote, byte inVelocity) {
    //dispNoteOn(inNote, 0);
    //Serial.print("handleNoteOn nt=");
    //Serial.println(inNote);

    /**
     * Repect the Midi channel ?
     */
    
    if (midiChannel == inChannel || LISTENTOALLCHANNELS) {
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
    
      uint16_t f = getFreq(inNote); // determine freq. from midi note
      if (waveForm != FM)
          setVoiceFreqency(f, slot, mil); // set the step value for the isr
      else
          setFMFrequencys(f, slot, mil);
      voice[slot].note = inNote;
      setGateOn(slot); // start adsr, gate on setGateOn(channel); // start adsr, gate on
      /*if (!doScope) {
        dispNoteOn(inNote, slot);
      }
      */
   }
}

void  print(String s, int val) {
  Serial.print(s);
  Serial.println(val);
}
// -------------------

/**
 * Find an active playing midi note in the voices array
 * @param note the note to look for
 * @return the voice-index or -1 if not found
 */
int16_t findOldestMidiNote(uint16_t note) { 
 // Serial.print("Find oldest Note to turn off n=");
 // Serial.print(note);
  
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
  // Serial.print(" Resulting slot=");
  // Serial.println(slot);
  return slot; 
}
//----------------------------------------


  //--------------
 
 

 /**
 * Here we create the actual sound
 * This method is called 40.000 time per second in the interrupt routine!
 */
void ICACHE_RAM_ATTR doVoice() {
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
           else if(waveForm == 3) {// square, use pwm value!
              int pv = pwm_CValue + pwm_value; 
              if (pv > 1020)
                pv = 1020;
              c = (tIdx < pv)?-2047:2047; 
           }
           voice[n].vOutput = c; //  +- 2047
          }
          addADSRStep(n, VOICES);     // 0-4096  all adsr ???
        }
    }
    // Analog Filter (LPF) AD:
    if (vAdsr[MAXVOICE].ADSR_mode != STOP && doFilterADSR) // disable filter adsr
      addADSRStep(MAXVOICE, FILTER);
      
    //-------------- Audio Mixer -----------------
    int32_t sum = 0;      
    anz = 0;
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
         anz++;   
      } 
    }
    
    //----------  Reverb Filter: recover values from the rev-buffer
    if (doReverbF && delayTime > 0 && !changes && diff == 0) {
      // blow up to original volume (*2)
      int8_t reverb = readRev() << 2; // echo value [+-127], since the echo buffer is only 8bit
      // reduce rev amplitude, by using the mixer value [0-4095]
      int32_t rv = (reverb * Avalue[P_REVAMOUNT]) >> 12; // : 4096; reduce echo amplitude by mixer value 4096=2^12, 
      if (rv != 0) { // if we have a pending echo
        
        // test echo value with scope:
       //pwmWrite(PWM_OUT, pwmVolume2+rv); // test output to 2nd analog out, just the reverb signal...
        //---
        sum += rv; // add to actual value, treat like another voice that got active
        anz++ ;
      }
    }
    

    
    //----------------- Synth Audio Mixer  --------------------
    if (anz > 0){
      int32_t graphicSum = sum >> 2;
      sum = sum >> 4;
      
     # if defined USE_CLIP_MIX   
      if (sum > pwmVolume2)
        sum = pwmVolume2;
      if (sum < -pwmVolume2)
        sum = -pwmVolume2;
     #endif  
      //Serial.print(" clipped sum=");
      //Serial.print(sum);
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
      //--- feed the delay buffer ----
      if (doReverbF && !changes) {
          int8_t rv = (int8_t) ((sum * 255) / pwmVolume2);
          writeHead(rv); //scale to 8bit, since our memory buffer only has 8 bit resolution
         // pwmWrite(PWM_OUT, pwmVolume2+rv);
      }
          
      // until here, all signals are +- values, so now add half amplitude to get positive values only:
      vres = graphicSum + 512;
      sum = pwmVolume2 + sum;
    
      //pwmWrite(PWM_OUT, sum); // res
      ledcWrite(ledChannel, sum);
      
      //digitalWrite(PC13, 0); // turn on, voice is active
    }
    else {
      // No voice active, feed the delay buffer with empty values
      if (doReverbF)
        writeHead(0);
      //
      //pwmWrite(PWM_OUT, pwmVolume2); 
      ledcWrite(ledChannel, pwmVolume2);
      vres = 512;
      //digitalWrite(PC13, 1);
    }
  }


  /**
 * Interrupt routine, triggered every 25us (40Khz)
 * In this routine, we travel along the wavetable and output the amplitude values we find.
 */
 void ICACHE_RAM_ATTR handler_Synth() {
  //portENTER_CRITICAL_ISR(&timerMux);

  globalTic++;            // count up all the time at 40khz
  if (globalTic > 39) {
    globalTic = 0;        // 40 tics at 40 khz is one millisec.
    mil++;                // increase millisecond tic, used for button debounce etc.
  }
  doVoice();              // the actual sound generation
  
  //portEXIT_CRITICAL_ISR(&timerMux);
 }

  /**
   * Update the display
   */
  void updateDisplay() {
   switch (page) {
    case BASE: // first page, shown on start-up
      if (requestToUpdate) {
        displayPage0(midiChannel, sNo);
      }
    break;
    case ADSRP:
      if (requestToUpdate) {
        drawADSR();
      }
      break;
    case SCOPE:
        /**
         * The dispaly refresh is not done every cycle...
         */
        refreh++;  
        if (refreh > 200) { 
          refreh = 0;
          requestToUpdate = true;
        } 
        tbaseidx++;
        if (tbaseidx > tbase) {
          dispIdx++; // 
          tbaseidx = 0;
        }
        dispCurve(vres, dispIdx, DisplayScale); // DisplayScale is what we scale the 128 hor. pixels to
        /**
         * if the end of the display is reached, clear the screen and start again
        */
        if (dispIdx > DisplayScale) {
         display.clearDisplay();
         dispIdx = 0;
         opx = 0;
        }
      break;
      case WAVESEL:
        if (requestToUpdate) {
          if (sNo == SAW)
           getSawWave(); // set the waveForm array
          else if (sNo == SIN) 
           getSinWave();
          else if (sNo == TRI) 
           getTriWave();
          /*else if (sNo == SQR) 
           getSQRWave();
           */
          drawWaveForm(sNo);
          if (sNo > SAW) 
              drawLeftArrow();
          if (sNo < (WMAX-1))
              drawRightArrow();
        }
      break;
   /*   case MIDISNIF:
          refreh++;  
          if (refreh > 1000) { 
            refreh = 0;
            requestToUpdate = true;
          } 
      break;
      */
      case DATTACK:
          if (requestToUpdate)
            drawAD(0);
          break;  
      case DDECAY:
        if (requestToUpdate)
            drawAD(1);
        break;
      case DPWM :
        if (requestToUpdate)
          drawPWM();
        break;
      
      case FMSTART :
        if (requestToUpdate)
          drawFMStart(v_start);
        break;
      case FMEND :
        if (requestToUpdate)
          drawFMEnd(v_end);
        break; 
      case FMDECAY :
      if (requestToUpdate)
        drawFMDecay(fm_decay);
        break;
  }

 
  if (requestToUpdate) {
    requestToUpdate = false;
    display.display();
  }
}







/**
 * Setup routine, this is the first to be called on startup of the stm32
 */
void setup() {
  // 1st Initialize the programm
  sNo = 3; // 0=saw, 1=sin,2=tri, 3=sqr, 4=FM

  fat =  256;
  fde =  512;

  pwm_value = 512;
  pw = pwm_value;

  v_start =  256;

  v_end = 64;

  fm_decay = 1023;

  initControls();
  initVoices();
  initGraphic();
  setAdsrVals(); // some values to beginn with. Maybe read from EEprom ?
  initRev(); // inít reverb filter
  requestToUpdate = true; // inital draw of the display

  midiChannel = 1;
  
  //pinMode(ONBOARDLED, OUTPUT); // on - board led etc
  //pinMode(scalePin, INPUT_ANALOG);
  pinMode(PWM_OUT, OUTPUT);

    delay(1000);
    Serial.begin(115200); // serial for pc communication


  /*
    # if !defined USE_BOOTLOADER  
      while (!Serial) { // this does not seem to work on my blue-pill, Serial is always true...
        delay(10);      // and with the boot-loader the pill stm never leaves this loop!
     }
    #endif*/
  
  /**
  * Pwm Set for analog out: 50kHz, 0-1420
 */
  /*HardwareTimer timer1 = HardwareTimer(1);  // using timer 1, can be 2,3,4...
  timer1.setPrescaleFactor(1);              // Pre-scale factor 1-65536
  timer1.setPeriod(pwm_cnt); //0-1420 @ 50kHz    // microseconds uSec 
  pinMode(PWM_OUT, PWM);      // synthesizer analog output
  */

  // configure LED PWM functionalitites
  ledcSetup(ledChannel, freq, resolution);
  
  // attach the channel to the GPIO2 to be controlled
  ledcAttachPin(PWM_OUT, ledChannel);


  /** 
   *  Setup Timer 2 to 25u (40000 per second)
   */

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, handler_Synth, true);
  timerAlarmWrite(timer, isrms, true);
  timerAlarmEnable(timer);

  /*
  Timer2.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
  Timer2.setPeriod(isr_cnt);                          // in microseconds (25u)
  Timer2.setCompare(TIMER_CH1, 1);                   
  Timer2.attachInterrupt(TIMER_CH1, handler_Synth);   // in this routine the synth moves through the wave-table
*/
  /**
   * Really only used when I developed this effect...
   */
  if (testRev) {
      // init the reverb-filter
      Serial.println("Start Rev test");
      setDelayTime(100); // [0-4096] = 0-120ms
      // Fill buffer with a small ramp
      for (int n = 0; n < 50; n++)
        writeHead(n);
      //delay(1000);
  }
  
  //---
  selectWave(sNo); // 1 = sin
  //displayPage0(midiChannel, sNo); // start screen
  
 // dispChannel(midiChannel, 0);
 // sei(); //Enable interrupt
 
  //digitalWrite(ONBOARDLED, 1); // turn LED off
  delay(200);

}



void loop() {
  readMIDI();

  /**
   * Only on start-up 
   */
  if (mil > 1000 && start) {
    
    setAdsrVals();
    start = false;
    testrun = 0;
  }


   
   updateDisplay();
   


  
  // -------- release notes in end of release state for re-use ------
  /**
   * If a voice is in finished release state mode (STOPPING), just clean it up here.
   * If we would do this in the interrupt routine, unexpected things might happen! :-)
   */
  for (int n=0; n < MAXVOICE; n++) {
   if (vAdsr[n].ADSR_mode == STOPPING) {
    int nt = voice[n].note;
    voice[n].note = 0;
    vAdsr[n].ADSR_mode = STOP;
   }
  }
  

  
}
