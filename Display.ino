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


// Used in scope function
// y: current amplitude of audio (0-1024)
// x: current x position
void dispCurve(uint16_t y, uint32_t x) {
  uint16_t h = DISP_HEIGHT - 1;
  
  uint16_t ym = map(y, 0, 1023, 0, 47);

  /*
  display.fillRect(x-1, 0, 2, DISP_HEIGHT, WHITE);
  display.drawLine(x-1, scopeYprev, x, ym, BLACK);
  */
  display.drawLine(x, 0, x, DISP_HEIGHT, WHITE);
  display.drawLine(x, scopeYprev, x, ym, BLACK);
  
  // set previous values for next line drawing
  scopeYprev = ym;
}

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

// set adsr values to display
void setAtValue(uint16_t a) {
  at = a;
}
void setDeValue(uint16_t a) {
  de = a;
}
void setSusValue(uint16_t a) {
  sus = a;
}
void setReValue(uint16_t a) {
  rel = a;
}



void drawADSR() {
 
    display.clearDisplay();
    uint16_t y = height -1;
    uint16_t x = 0;
    uint32_t s = sus * 64 / 4096; // convert to pixel

    //display.setTextSize(2);
    //display.setCursor(80, 0);
    //display.println("ADSR"); 
    /*Serial.print("at=");
    Serial.println(at);
    */
    // Attack
    uint32_t a = at * 32 / 4096; // after what pixel we are at 100%
   // Serial.println(a);
    display.drawLine(x, height, a, 0, BLACK);
    display.drawLine(a, 0, 32, 0, BLACK);
    // Decay & sustain
    x = 32;
    a = de * 32 / 4096; // 
    y = height - s -1;
    display.drawLine(x, 0, x+a, y, BLACK);
    display.drawLine(x+a, y, x+64, y, BLACK);
    x = 96;
    // release
    a = rel * 32 / 4096; //
    display.drawLine(x, y, x+a, height-1, BLACK);
    display.drawLine(x+a, height-1, 128, height-1, BLACK);
    
}


void drawPWM() {
    display.clearDisplay();
    uint16_t y = height -1;
    uint16_t x = 0;
    uint16_t h = 20;
    display.setTextSize(1);
    display.setCursor(40, 0);
    display.println("PWM"); 
    uint16_t a = pw * 127 / 1023; // // table length
    display.drawLine(x, y, x, h, BLACK);
    display.drawLine(x, h, a, h, BLACK);
    display.drawLine(a, h, a, y, BLACK); 
    display.drawLine(a, y, 127, y, BLACK);
      
}
void dispFM() {
  display.clearDisplay();
  int y = noOnY+LF;
  display.setCursor(ntOnX,y);
  // clear old text
  display.fillRect(ntOnX, y, ntW, LF, 0);
  //display.display();
  display.print("FM");
  //display.println(nt);
  //display.display();
}



/**
 * Scale the table to the oled display with 128 x 64 pixel
 */
void getSawWave() {
  for (int n=0; n < WAVEDISP_WIDTH; n++) {
    uint16_t idx = (n * SawLengthInt) / WAVEDISP_WIDTH;
    waveFormD[n] = 31+ (getSawInt(idx) >> 6); // table = 0-255; display = 0-63
  }
}

void getSinWave() {
  for (int n=0; n < WAVEDISP_WIDTH; n++) {
    uint16_t idx = (n * SinusLengthInt) / WAVEDISP_WIDTH;
    waveFormD[n] = 31+ (getSinInt(idx) >> 6); // table = 0-255; display = 0-63
  }
}
void getTriWave() {
  for (int n=0; n < WAVEDISP_WIDTH; n++) {
    uint16_t idx = (n * TriLengthInt) / WAVEDISP_WIDTH;
    waveFormD[n] = 31+ (getTriInt(idx) >> 6); // table = 0-255; display = 0-63
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
  int h = height -1;
  if (w == SQR) {
    //Serial.print("pwm_value="); 
    //Serial.print(pwm_value);
    
    uint32_t pwmx = pwm_value * WAVEDISP_WIDTH;
    //Serial.print(" z=");
    //Serial.print(pwmx);
    pwmx = xstart + (pwmx >> 10); // : 1024 (0-1024 is the range for the pwm) 
    //Serial.print(" pwmx=");
    //Serial.println(pwmx);
    
    display.drawLine(xstart, 0, pwmx, 0, BLACK);
    display.drawLine(pwmx, 0, pwmx, h, BLACK);
    display.drawLine(pwmx, h, xstart + WAVEDISP_WIDTH, h, BLACK);
  }
  else if (w == FM) {
    dispFM();
  }
  else {
    for (int n = 0; n < WAVEDISP_WIDTH; n++)
      display.drawPixel(xstart+n, h - waveFormD[n], BLACK);
  }
}

void drawTBI(char* text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.println("TODO:"); 
  display.setCursor(20, 30);
  display.println(text); 
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
           getSawWave(); // set the waveForm array
          else if (sNo == SIN) 
           getSinWave();
          else if (sNo == TRI) 
           getTriWave();
          /*else if (sNo == SQR) 
           getSQRWave();
           */
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
            drawTBI("FM");
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
