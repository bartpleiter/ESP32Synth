//#include "stm32-WaveTableSynth_a.h"
//#include "Graphic.h" 
//#include "ADSR.h"

//midi buffer vars:
uint8_t mBuffer[3];
uint8_t mIdx = 0;
uint8_t cmdLen =1;
uint8_t command = 0;


// to set all notes off (in case I want to map this to a certain control value):
//  handleAllNoteOff();

/**
 * Execute received Midi command 
 * Control values are usually in the range of 0-127, so we can shift it to the left by 5 for simple scaling to 0-~4095
 */
void executeMidi() {
    // received 3 bytes
    /*
    Serial.print(command, HEX);
    Serial.print(" ");
    Serial.print(mBuffer[0], HEX);
    Serial.print(" ");
    Serial.print(mBuffer[1], HEX);
    Serial.println();
    */


    // note on
    if (command >= 0x90 && command <= 0x9f) 
    {
      handleNoteOn((command & 0x0f) + 1, mBuffer[0], mBuffer[1]); // channel, note, velocity
    }
    
    // note off
    else if (command >= 0x80 && command <= 0x8f) 
    {
      handleNoteOff((command & 0x0f) + 1, mBuffer[0], 0);
    }
    
    // control change
    else if (command >= 0xb0 && command <= 0xbf) 
    {
      // modulation wheel
      if ( mBuffer[0] == 0x01) {

        if (page == PAGE_WAVE) {
          // select a new waveform!
          sNo = map(mBuffer[1], 0, 127, 0, WMAX-1);
          selectWave(sNo);
        }
        else if (page == PAGE_PWM) {
          pw = map(mBuffer[1], 0, 127, 10, 1020);
          pwm_value = pw;
        }

        requestToUpdate = true;
      }
      
      // volume slider
      else if ( mBuffer[0] == 0x07) {
        
      }

      // r1 slider
      else if ( mBuffer[0] == 0x4a) {
        if (page == PAGE_ADSR)
        {
          setAttackScale(mBuffer[1] << 5);
          setAtValue(mBuffer[1] << 5); // currently for graphics functions
          requestToUpdate = true;
        }
      }

      // r2 slider
      else if ( mBuffer[0] == 0x47) {
        if (page == PAGE_ADSR)
        {
          setDecayScale(mBuffer[1] << 5);
          setDeValue(mBuffer[1] << 5);
          requestToUpdate = true;
        }
      }

      // r3 slider
      else if ( mBuffer[0] == 0x49) {
        if (page == PAGE_ADSR)
        {
          setSustainValue(mBuffer[1] << 5);
          setSusValue(mBuffer[1] << 5);
          requestToUpdate = true;
        }
      }

      // r4 slider
      else if ( mBuffer[0] == 0x48) {
        if (page == PAGE_ADSR)
        {
          setReleaseScale(mBuffer[1] << 5);
          setReValue(mBuffer[1] << 5);
          requestToUpdate = true;
        }
      }
    }

    // pitch bend
    else if (command >= 0xe0 && command <= 0xef) 
    {
      // change page
      if (mBuffer[1] == 127)
      {
        page++;
        if (page >= PAGES)
          page = 0;

        requestToUpdate = true;
      }
      else if (mBuffer[1] == 0)
      {
        if (page == 0)
          page = PAGES-1;
        else
          page--;

        requestToUpdate = true;
      }
   
    }
   
}


/**
 * Check if this byte is a command byte
 * @param the byte we just read
 * @return the length of the expected command, if it is a command byte ! Else return 0
 */
int isCommand(uint8_t b ) {
  int l = 0;
  if (b >= 0x80 && b <= 0x8f) { // Note off
    l = 2; 
    command = b;
  }
  else if (b >= 0x90 && b <= 0x9f) { // Note on
    l = 2;
    command = b;
  }
  else if (b >= 0xa0 && b <= 0xaf) {
     // aftertouch, poly
    l = 2;
    command = b;
  }
  else if (b >= 0xb0 && b <= 0xbf) {
     // control change
    l = 2;
    command = b;
  }
  else if (b >= 0xc0 && b <= 0xcf) {
     // program change
    l = 1;
    command = b;
  }
  else if (b >= 0xd0 && b <= 0xdf) {
     // aftertouch, channel
    l = 1;
    command = b;
  }
  else if (b >= 0xe0 && b <= 0xef) {
     // Pitch wheel
    l = 2;
    command = b;
  }
  else if (b >= 0xf0 && b <= 0xff) {
     // System exclusive
    l = 1; // this may be wrong, change per manufacturer
    command = b;
  }
  return l;
}


/**
 * Read one byte from the serial stream
 */
 //TODO: optimize len and cmdLen
  void doRead() {
    uint8_t b = Serial.read();
    
    // display the byte on the display
    //printHex(b);

    int len = isCommand(b );
    if (len > 0){
      cmdLen = len;
      mIdx = 0; // reset buffer index
    }
    else {
      // is a data-byte 
      mBuffer[mIdx++] = b;
      cmdLen--;
      if (cmdLen == 0) {
        // all data byte are read, now execute the command
         executeMidi();
      }
    }
  }


void readMIDI(){
   if (Serial.available())
      doRead();
  }
