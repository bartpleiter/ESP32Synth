

 
void initGraphic() {
    
  display.begin();
  display.setContrast(63); // 63 seems to work fine for my 5110 display
  display.setRotation(2); // rotate 180 degrees, since that is the way the display is facing in my setup
  //delay(2000);
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
}

void printHex(byte b) {
  display.print("0x");
  display.print(b, HEX);
  display.print(",");
  cx++;
  if (cx > 3) {
    cx = 0;
    
    display.println("");
    cy++;
    if (cy > 7) {
      display.clearDisplay();
      cy == 0;
    }
    display.setCursor(0, cy*8);
  }
  
}

/**
 * map to 128 * 64 pixel display
 * @param y in the range of 0-1024
 * @param x in the range of 0-4000;
 * 
 */
void dispCurve(uint16_t y, uint32_t x, uint16_t max) {
  uint16_t h = height - 1;
  uint16_t xp = (uint16_t) (x & 0xffff) * width / max;
  uint16_t yp = (y * h) >> 10;
  display.drawLine(opx, h - opy, xp, h - yp, BLACK);
  opx = xp;
  opy = yp;
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
      
  }

 
  if (requestToUpdate) {
    requestToUpdate = false;
    display.display();
  }
}
