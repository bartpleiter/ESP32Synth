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
      handleNoteOn((command & 0x0f), mBuffer[0], mBuffer[1]); // channel, note, velocity
    }
    
    // note off
    else if (command >= 0x80 && command <= 0x8f) 
    {
      handleNoteOff((command & 0x0f), mBuffer[0], 0);
    }
    
    // control change
    else if (command >= 0xb0 && command <= 0xbf) 
    {
      // modulation wheel
      if ( mBuffer[0] == 0x01) {

        if (page == PAGE_WAVE) {
          // select a new waveform!
          waveForm = map(mBuffer[1], 0, 127, 0, WMAX-1);
          sendFrameBuffer = true;
        }
        else if (page == PAGE_PWM) {
          pwmWidth = map(mBuffer[1], 0, 127, 0, 2047);
          sendFrameBuffer = true;
        }
        else if (page == PAGE_SCOPE) {
          scopeDelay = map(mBuffer[1], 0, 127, 0, 1000);
        }
      }
      
      // volume slider
      else if ( mBuffer[0] == 0x07) {
        pwmWidth = map(mBuffer[1], 0, 127, 0, 2047);
      }

      // r1 slider
      else if ( mBuffer[0] == 0x4a) {
        if (page == PAGE_ADSR)
        {
          setAttackScale(mBuffer[1]);
          DisplaySetAtValue(mBuffer[1]); // currently for graphics functions
          sendFrameBuffer = true;
        }
      }

      // r2 slider
      else if ( mBuffer[0] == 0x47) {
        if (page == PAGE_ADSR)
        {
          setDecayScale(mBuffer[1]);
          DisplaySetDeValue(mBuffer[1]);
          sendFrameBuffer = true;
        }
      }

      // r3 slider
      else if ( mBuffer[0] == 0x49) {
        if (page == PAGE_ADSR)
        {
          Sval =mBuffer[1] << 5;
          DisplaySetSusValue(mBuffer[1] << 5);
          sendFrameBuffer = true;
        }
      }

      // r4 slider
      else if ( mBuffer[0] == 0x48) {
        if (page == PAGE_ADSR)
        {
          setReleaseScale(mBuffer[1]);
          DisplaySetReValue(mBuffer[1]);
          sendFrameBuffer = true;
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

        sendFrameBuffer = true;
      }
      else if (mBuffer[1] == 0)
      {
        if (page == 0)
          page = PAGES-1;
        else
          page--;

        sendFrameBuffer = true;
      }
   
    }
   
}


int isCommand(uint8_t b ) {
  int l = 0;
  if (b >= 0x80 && b <= 0x8f) { 
    // Note off
    l = 2; 
    command = b;
  }
  else if (b >= 0x90 && b <= 0x9f) { 
    // Note on
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
    l = 1;
    command = b;
  }
  return l;
}


  void doRead() {
    uint8_t b = Serial.read();

    int len = isCommand(b );
    if (len > 0){
      cmdLen = len;
      mIdx = 0;
    }
    else {
      mBuffer[mIdx++] = b;
      cmdLen--;
      if (cmdLen == 0) {
         executeMidi();
      }
    }
  }


void readMIDI(){
   if (Serial.available())
      doRead();
  }
