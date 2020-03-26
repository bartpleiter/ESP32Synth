/*
 * DISPLAY
 * Functions for drawing info on the 5110 display
 */

// Initialize display
// is called before starting the first interrupt
void initDisplay() 
{
  display.begin();
  display.setContrast(63);  // 63 seems to be okay for my 5110 display
  display.setRotation(2);   // rotate 180 degrees, since that is the way the display is facing in my setup
  display.setTextSize(1);
  display.clearDisplay();
  display.display();
  sendFrameBuffer = true;   // draw the first menu page as fast as possible
}


// Draw next vertical line (will look like a curve) in audio scope
// y: current amplitude of audio
// x: current x position
void dispCurve(uint32_t y, uint32_t x) 
{
  uint32_t ym = map(y, 0, 256, 0, 47);              // map the aplitude to the height of the display
  display.drawLine(x, 0, x, DISP_HEIGHT-1, WHITE);  // clear the line of the previous frame
  display.drawLine(x, scopeYprev, x, ym, BLACK);
  
  // set previous values for next line drawing
  scopeYprev = ym;
}


// Draws and displays audio scope
// should be called many times per second
void drawScope()
{
  dispCurve(scopeAmpl, scopeX);
  scopeX++;
  // wrap around when at end of display
  if (scopeX >= DISP_WIDTH) 
  {
    scopeX = 0;
    scopeRefreshCounter++;
    // decide if we want to send the framebuffer to the display
    if (mil - scopeRefreshCounter >= scopeDelay) 
    { 
      scopeRefreshCounter = mil;
      sendFrameBuffer = true;
    }   
  }  
}

// Set ADSR display values
// inputs values range from 0 to MIDICONTROLMAXVAL
void setDisplayAttack(uint32_t val) 
{
  dispAttack =  map(val, 0, MIDICONTROLMAXVAL, 0, 36);
}

void setDisplayDecay(uint32_t val) 
{
  dispDecay =   map(val, 0, MIDICONTROLMAXVAL, 0, 20);
}

void setDisplaySustain(uint32_t val) 
{
  dispSustain = map(val, 0, MIDICONTROLMAXVAL, 0, 40);
}

void setDisplayRelease(uint32_t val) 
{
  dispRelease = map(val, 0, MIDICONTROLMAXVAL, 0, 20);
}


// Draw the ADSR graph on screen
void drawADSR() 
{
  // start by clearing the display
  display.clearDisplay();

  // draw page title
  display.setCursor(32, 0);
  display.println("ADSR"); 
  
  // draw Attack line
  display.drawLine(0, DISP_HEIGHT-1, dispAttack, DISP_ADSR_HEIGHT_OFFSET -1, BLACK);
  
  // draw Decay line
  display.drawLine(dispAttack, DISP_ADSR_HEIGHT_OFFSET -1, dispAttack + dispDecay, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, BLACK);
  
  // draw Sustain line
  display.drawLine(dispAttack + dispDecay, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, 
                    dispAttack + dispDecay + 20, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) - 1, BLACK);
  
  // draw Release line
  display.drawLine(dispAttack + dispDecay + 20, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, 
                    dispAttack + dispDecay + 20 + dispRelease, DISP_HEIGHT-1 , BLACK);
}


// Draw width for square wave
void drawPWM() {
  // start by clearing the display
  display.clearDisplay();

  // draw page title
  display.setCursor(34, 0);
  display.println("PWM"); 

  // map pwm width to display width
  int mappedPwmWidth = 83 - map(pwmWidth, 0, 2047, 0, 83);

  // draw square wave based on (mapped)pwmWidth
  display.drawLine(0, DISP_HEIGHT-1, mappedPwmWidth, DISP_HEIGHT-1, BLACK);
  display.drawLine(mappedPwmWidth, DISP_HEIGHT-1, mappedPwmWidth, 12, BLACK);
  display.drawLine(mappedPwmWidth, 12, DISP_WIDTH-1, 12, BLACK); 
}


// Draw the ADSR states of all voices (max 20 will fit on screen)
void drawVoices()
{
  //draw and send 30 times per second
  if (mil - milPrev >= 33) // ~30fps
  {
    milPrev = mil;
    sendFrameBuffer = true;

    // clear screen
    display.clearDisplay();
  
    // draw page title
    display.setCursor(26, 0);
    display.println("Voices"); 
  
    // draw labels in small font at left of screen
    display.setFont(&Picopixel);
    display.setCursor(0, 14);
    display.println("I"); 
    
    display.setCursor(0, 21);
    display.println("A"); 
    
    display.setCursor(0, 28);
    display.println("D"); 
    
    display.setCursor(0, 35);
    display.println("S"); 
    
    display.setCursor(0, 42);
    display.println("R"); 
    display.setFont(NULL);
  
    // loop through all voices and draw the ADSR states on screen
    for (uint32_t n=0; n < MAXVOICES; n++) 
    {
      switch (voices[n].adsr.state) 
      {
        case ADSR_IDLE:
          display.fillRect((n<<2)+5, 10, 2, 5, BLACK);
          break;
        case ADSR_ATTACK:
          display.fillRect((n<<2)+5, 17, 2, 5, BLACK);
          break;
        case ADSR_DECAY:
          display.fillRect((n<<2)+5, 24, 2, 5, BLACK);
          break;
        case ADSR_SUSTAIN:
          display.fillRect((n<<2)+5, 31, 2, 5, BLACK);
          break;
        case ADSR_RELEASE:
          display.fillRect((n<<2)+5, 38, 2, 5, BLACK);
          break;
      }
    }
  }
}


// Functions to map saw, sine and triangle wave tables to the size of the display
void getSawWave() 
{
  for (uint32_t n=0; n < DISP_WIDTH; n++) 
  {
    uint32_t idx = (n * WAVETABLELENGTH) / DISP_WIDTH;
    scopeWaveForm[n] = (DISP_HEIGHT/2) + (getSawInt(idx) >> 7) - 6;
  }
}

void getSinWave() 
{
  for (uint32_t n=0; n < DISP_WIDTH; n++) 
  {
    uint32_t idx = (n * WAVETABLELENGTH) / DISP_WIDTH;
    scopeWaveForm[n] = (DISP_HEIGHT/2) + (getSinInt(idx) >> 7) - 6;
  }
}

void getTriWave() 
{
  for (uint32_t n=0; n < DISP_WIDTH; n++) 
  {
    uint32_t idx = (n * WAVETABLELENGTH) / DISP_WIDTH;
    scopeWaveForm[n] = (DISP_HEIGHT/2) + (getTriInt(idx) >> 7) - 6;
  }
}


// Draw selected waveform on display
void drawWaveForm() {
  // clear screen
  display.clearDisplay();

  // draw page title
  display.setCursor(32, 0);
  display.println("Wave"); 
  
  if (waveForm == SQR) 
  {
    display.drawLine(0,             12,                DISP_WIDTH/2, 12,            BLACK);
    display.drawLine(DISP_WIDTH/2,  12,                DISP_WIDTH/2, DISP_HEIGHT-1, BLACK);
    display.drawLine(DISP_WIDTH/2,  DISP_HEIGHT-1    , DISP_WIDTH,   DISP_HEIGHT-1, BLACK);
  }
  else if (waveForm == FM) 
  {
    // only print FM, since FM waves are not drawable
    display.setTextSize(2);
    display.setCursor(32, 20);
    display.println("FM"); 
    display.setTextSize(1);
  }
  else 
  {
    for (uint32_t n = 0; n < DISP_WIDTH; n++)
    {
      display.drawPixel(n, DISP_HEIGHT - scopeWaveForm[n], BLACK);
    }
  }
}


// Placeholder for to be implemented functions
void drawTBI(char* text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.println("TODO:"); 
  display.setCursor(20, 30);
  display.println(text); 
}


// Update the display
void updateDisplay() 
{
  // draw different things based on the selected menu page
  switch (page) 
  {
    case PAGE_WAVE:
        if (sendFrameBuffer) 
        {
          if (waveForm == SAW)
            getSawWave();
          else if (waveForm == SIN) 
            getSinWave();
          else if (waveForm == TRI) 
            getTriWave();
          drawWaveForm();
        }
        break;
    
    case PAGE_ADSR:
        if (sendFrameBuffer)
          drawADSR();
        break;
    
    case PAGE_FM:
        if (sendFrameBuffer)
          drawTBI("FM");
        break;  
    
    case PAGE_LFO:
        if (sendFrameBuffer)
          drawTBI("LFO");
        break;
    
    case PAGE_REVERB:
        if (sendFrameBuffer)
          drawTBI("REVERB");
        break;
    
    case PAGE_PWM:
        if (sendFrameBuffer)
          drawPWM();
        break;
    
    case PAGE_PASS:
        if (sendFrameBuffer)
          drawTBI("PASS");
        break;
    
    case PAGE_VOICES:
        drawVoices();
        break;
    
    case PAGE_SCOPE:
        drawScope();
        break;
  }
  
  // send the framebuffer when requested
  if (sendFrameBuffer) 
  {
    sendFrameBuffer = false;
    display.display();
  }
}
