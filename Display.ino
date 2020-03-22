/*
 * DISPLAY
 * Functions for drawing info on the 5110 display
 */

// initialize display
// is called before starting the first interrupt
void initDisplay() 
{
  display.begin();
  display.setContrast(63); // 63 seems for my 5110 display
  display.setRotation(2); // rotate 180 degrees, since that is the way the display is facing in my setup
  display.setTextSize(1);
  display.clearDisplay();
  // TODO future: could add a splash "boot" screen here using delay
  display.display();
}


// Draw next vertical line in audio scope
// y: current amplitude of audio
// x: current x position
void dispCurve(uint16_t y, uint16_t x) 
{
  uint16_t h = DISP_HEIGHT - 1;
  uint16_t ym = map(y, 0, 1023, 0, 47);
  display.drawLine(x, 0, x, DISP_HEIGHT-1, WHITE);
  display.drawLine(x, scopeYprev, x, ym, BLACK);
  
  // set previous values for next line drawing
  scopeYprev = ym;
}

// Draws and displays audio scope
// Should be called in main loop
void drawScope()
{
  dispCurve(vres, scopeX);
  scopeX++;
  if (scopeX >= DISP_WIDTH) 
  {
    scopeX = 0;
    scopeRefreshCounter++;
    if (mil - scopeRefreshCounter >= scopeDelay) 
    { 
      scopeRefreshCounter = mil;
      requestToUpdate = true;
    }   
  }  
}

// Set ADSR display values
// inputs values range from 0 to 4095
void DisplaySetAtValue(uint16_t val) {
  dispAttack = map(val, 0, 4095, 0, 36);
}
void DisplaySetDeValue(uint16_t val) {
  dispDecay = map(val, 0, 4095, 0, 20);
}
void DisplaySetSusValue(uint16_t val) {
  dispSustain = map(val, 0, 4095, 0, 40);;
}
void DisplaySetReValue(uint16_t val) {
  dispRelease = map(val, 0, 4095, 0, 20);
}

void drawADSR() {
    display.clearDisplay();

    display.setCursor(32, 0);
    display.println("ADSR"); 

    // Attack
    display.drawLine(0, DISP_HEIGHT-1, dispAttack, DISP_ADSR_HEIGHT_OFFSET, BLACK);
    
    // Decay
    display.drawLine(dispAttack, DISP_ADSR_HEIGHT_OFFSET, dispAttack + dispDecay, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, BLACK);

    // Sustain
    display.drawLine(dispAttack + dispDecay, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, 
                      dispAttack + dispDecay + 20, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) - 1, BLACK);

    // Relea
    display.drawLine(dispAttack + dispDecay + 20, DISP_ADSR_HEIGHT_OFFSET + (40 - dispSustain) -1, 
                      dispAttack + dispDecay + 20 + dispRelease, DISP_HEIGHT-1 , BLACK);
}


void drawPWM() {
  //todo pwmWidth use
    display.clearDisplay();

    display.setCursor(34, 0);
    display.println("PWM"); 
    
    int mappedPwmWidth = 83 - map(pwmWidth, 0, 2047, 0, 83);
    
    display.drawLine(0, DISP_HEIGHT-1, mappedPwmWidth, DISP_HEIGHT-1, BLACK);
    display.drawLine(mappedPwmWidth, DISP_HEIGHT-1, mappedPwmWidth, 12, BLACK);
    display.drawLine(mappedPwmWidth, 12, DISP_WIDTH-1, 12, BLACK); 

      
}




/**
 * Scale the table to the oled display with 128 x 64 pixel
 */
void getSawWave() {
  for (int n=0; n < DISP_WIDTH; n++) {
    uint16_t idx = (n * SawLengthInt) / DISP_WIDTH;
    waveFormD[n] = (DISP_HEIGHT/2) + (getSawInt(idx) >> 7) - 6;
  }
}

void getSinWave() {
  for (int n=0; n < DISP_WIDTH; n++) {
    uint16_t idx = (n * SinusLengthInt) / DISP_WIDTH;
    waveFormD[n] = (DISP_HEIGHT/2) + (getSinInt(idx) >> 7) - 6;
  }
}
void getTriWave() {
  for (int n=0; n < DISP_WIDTH; n++) {
    uint16_t idx = (n * TriLengthInt) / DISP_WIDTH;
    waveFormD[n] = (DISP_HEIGHT/2) + (getTriInt(idx) >> 7) - 6;
  }
}
/*void getSQRWave() {
  
    for (int n=0; n < WAVEDISP_WIDTH; n++) {
      uint16_t idx = (n * TriLength) / WAVEDISP_WIDTH;
      waveFormD[n] = (idx < pwm_value)?63:0; 
    }
  
}
*/
void drawWaveForm(int w) {
  display.clearDisplay();

  display.setCursor(32, 0);
  display.println("Wave"); 
  

  if (w == SQR) {
    display.drawLine(0,             12,                DISP_WIDTH/2, 12,            BLACK);
    display.drawLine(DISP_WIDTH/2,  12,                DISP_WIDTH/2, DISP_HEIGHT-1, BLACK);
    display.drawLine(DISP_WIDTH/2,  DISP_HEIGHT-1    , DISP_WIDTH,   DISP_HEIGHT-1, BLACK);
  }
  else if (w == FM) {
    display.setTextSize(2);
    display.setCursor(32, 20);
    display.println("FM"); 
    display.setTextSize(1);
  }
  else {
    for (int n = 0; n < DISP_WIDTH; n++)
      display.drawPixel(n, DISP_HEIGHT - waveFormD[n], BLACK);
  }
}


// placeholder for to be implemented functions
void drawTBI(char* text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.println("TODO:"); 
  display.setCursor(20, 30);
  display.println(text); 
}

void drawFM()
{
  display.clearDisplay();

  display.setCursor(36, 0);
  display.println("FM"); 

  display.setCursor(0, 14);
  display.println("MOD"); 
  display.setCursor(60, 14);
  display.println(fm_modulator); 


  display.setCursor(0, 22);
  display.println("START"); 
  display.setCursor(60, 22);
  display.println(v_start); 
  

  display.setCursor(0, 30);
  display.println("END"); 
  display.setCursor(60, 30);
  display.println(v_end); 
  

  display.setCursor(0, 38);
  display.println("DECAY"); 
  display.setCursor(60, 38);
  display.println(fm_decay); 
  
}


/**
   * Update the display
   */
  void updateDisplay() {
   switch (page) {
    case PAGE_WAVE:
      if (requestToUpdate) 
      {
        if (sNo == SAW)
          getSawWave();
        else if (sNo == SIN) 
          getSinWave();
        else if (sNo == TRI) 
          getTriWave();
        drawWaveForm(sNo);
      }
      break;
      
    case PAGE_ADSR:
      if (requestToUpdate) {
        drawADSR();
      }
      break;
     

      
      case PAGE_FM:
          if (requestToUpdate)
            drawFM();
          break;  

      case PAGE_LFO:
          if (requestToUpdate)
            drawTBI("LFO");
          break;

      case PAGE_REVERB:
          if (requestToUpdate)
            drawTBI("REVERB");
          break;
          
      case PAGE_PWM:
          if (requestToUpdate)
            drawPWM();
          break;
          
      case PAGE_PASS:
          if (requestToUpdate)
            drawTBI("PASS");
          break;



      case PAGE_SCOPE:
        drawScope();
      break;
      
  }

 
  if (requestToUpdate) {
    requestToUpdate = false;
    display.display();
  }
}
