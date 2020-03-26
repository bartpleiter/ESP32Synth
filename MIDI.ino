/*
 * MIDI
 * Contains all MIDI related functions
 * Please note that this is all designed for an Acorn Masterkey (61) keyboard
 * So if you have another keyboard, you should update the executeMidi function with your own MIDI codes
 */


// Execute the received MIDI command
void executeMidi() 
{
  // note on
  if (command >= 0x90 && command <= 0x9f) 
  {
    handleNoteOn((command & 0x0f), mBuffer[0], mBuffer[1]); // channel, note, velocity
  }
  
  // note off
  else if (command >= 0x80 && command <= 0x8f) 
  {
    handleNoteOff((command & 0x0f), mBuffer[0], 0); // channel, note, velocity
  }
  
  // control change
  else if (command >= 0xb0 && command <= 0xbf) 
  {
    // modulation wheel
    if ( mBuffer[0] == 0x01) 
    {
      if (page == PAGE_WAVE) 
      {
        // set waveform
        waveForm = map(mBuffer[1], 0, 127, 0, WMAX-1);
        sendFrameBuffer = true;
      }
      else if (page == PAGE_PWM) 
      {
        // set PWM width
        pwmWidth = map(mBuffer[1], 0, 127, 0, 2047);
        sendFrameBuffer = true;
      }
      else if (page == PAGE_SCOPE)
      {
        // set scope delay
        scopeDelay = mBuffer[1] << 2;
      }
    }
    
    // volume slider
    else if ( mBuffer[0] == 0x07) 
    {
      // currently set PWM width, should become something else in the future
      pwmWidth = map(mBuffer[1], 0, 127, 0, 2047);
    }

    // C1 knob
    else if ( mBuffer[0] == 0x4a) 
    {
      // set attack
      if (page == PAGE_ADSR || page == PAGE_VOICES || page == PAGE_WAVE || page == PAGE_SCOPE || page == PAGE_PWM)
      {
        setADSRattack     (mBuffer[1]);
        setDisplayAttack  (mBuffer[1]);
        sendFrameBuffer = true;
      }
    }

    // C2 knob
    else if ( mBuffer[0] == 0x47) 
    {
      // set decay
      if (page == PAGE_ADSR || page == PAGE_VOICES || page == PAGE_WAVE || page == PAGE_SCOPE || page == PAGE_PWM)
      {
        setADSRdecay      (mBuffer[1]);
        setDisplayDecay   (mBuffer[1]);
        sendFrameBuffer = true;
      }
    }

    // C3 knob
    else if ( mBuffer[0] == 0x49) 
    {
      // set sustain
      if (page == PAGE_ADSR || page == PAGE_VOICES || page == PAGE_WAVE || page == PAGE_SCOPE || page == PAGE_PWM)
      {
        setADSRsustain     (mBuffer[1]);
        setDisplaySustain  (mBuffer[1]);
        sendFrameBuffer = true;
      }
    }

    // C4 knob
    else if ( mBuffer[0] == 0x48) 
    {
      // set release
      if (page == PAGE_ADSR || page == PAGE_VOICES || page == PAGE_WAVE || page == PAGE_SCOPE || page == PAGE_PWM)
      {
        setADSRrelease     (mBuffer[1]);
        setDisplayRelease  (mBuffer[1]);
        sendFrameBuffer = true;
      }
    }
  }

  // pitch bend
  else if (command >= 0xe0 && command <= 0xef) 
  {
    // change menu page when pich bend is at top or bottom
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


// Checks if MIDI byte b is a command
// Returns the number of expected arguments if b is a command
// Else it returns 0
uint32_t isCommand(uint8_t b ) 
{
  uint32_t l = 0;
  if (b >= 0x80 && b <= 0x8f) 
  { 
    // Note off
    l = 2; 
    command = b;
  }
  else if (b >= 0x90 && b <= 0x9f) 
  { 
    // Note on
    l = 2;
    command = b;
  }
  else if (b >= 0xa0 && b <= 0xaf) 
  {
    // aftertouch, poly
    l = 2;
    command = b;
  }
  else if (b >= 0xb0 && b <= 0xbf) 
  {
    // control change
    l = 2;
    command = b;
  }
  else if (b >= 0xc0 && b <= 0xcf) 
  {
    // program change
    l = 1;
    command = b;
  }
  else if (b >= 0xd0 && b <= 0xdf) 
  {
    // aftertouch, channel
    l = 1;
    command = b;
  }
  else if (b >= 0xe0 && b <= 0xef) 
  {
    // Pitch wheel
    l = 2;
    command = b;
  }
  else if (b >= 0xf0 && b <= 0xff) 
  {
    // System exclusive
    l = 1;
    command = b;
  }
  return l;
}


// Reads and parses MIDI byte from serial buffer
// Executes the MIDI command when all arguments are received
void doRead() 
{
  uint8_t b = Serial.read();
  
  uint32_t len = isCommand(b);
  if (len > 0)
  {
    cmdLen = len;
    mIdx = 0;
  }
  else 
  {
    mBuffer[mIdx++] = b;
    cmdLen--;
    if (cmdLen == 0) 
    {
      executeMidi();
    }
  }
}


// Checks if MIDI bytes are avaiable and reads/executes them
void checkMIDI()
{
  while (Serial.available())
    doRead();
}
