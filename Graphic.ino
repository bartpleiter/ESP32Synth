

 
void initGraphic() {
  //display.begin(SH1106_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)
  
  display.begin();
  display.setContrast(63); // 63 seems to work fine for my display
  display.invertDisplay(1); // invert because we swapped BLACk and WHITE definitions
  display.display();
  //delay(2000);
  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
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
  display.drawLine(opx, h - opy, xp, h - yp, WHITE);
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

// set adsr values to display
void setFAtValue(uint16_t a) {
  fat = a;
}
void setFDeValue(uint16_t a) {
  fde = a;
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
    display.drawLine(x, height, a, 0, WHITE);
    display.drawLine(a, 0, 32, 0, WHITE);
    // Decay & sustain
    x = 32;
    a = de * 32 / 4096; // 
    y = height - s -1;
    display.drawLine(x, 0, x+a, y, WHITE);
    display.drawLine(x+a, y, x+64, y, WHITE);
    x = 96;
    // release
    a = rel * 32 / 4096; //
    display.drawLine(x, y, x+a, height-1, WHITE);
    display.drawLine(x+a, height-1, 128, height-1, WHITE);
    
}

void drawAD(uint16_t mode) {
 
    display.clearDisplay();
    uint16_t y = height -1;
    uint16_t x = 0;
    uint32_t a = fat * 64 / 4095; // convert to pixel
    uint16_t h = 20;
    display.setTextSize(2);
    display.setCursor(40, 0);
    if (mode == 0)
      display.println("Attack"); 
    else 
      display.println("Decay"); 
    /*Serial.print("at=");
    Serial.println(at);
    */
    // Attack
   // Serial.println(a);
    display.drawLine(x, height, a, h, WHITE);
    display.drawLine(a, h, 64, h, WHITE);
    uint16_t s = 1;
    // Decay 
    x = 64;
    a = fde * 64 / 4095; // 
    y = height - s -1;
    display.drawLine(x, h, x+a, y, WHITE);
    display.drawLine(x+a, y, 128, y, WHITE);
    
}


void drawPWM() {
    display.clearDisplay();
    uint16_t y = height -1;
    uint16_t x = 0;
    uint16_t h = 20;
    display.setTextSize(2);
    display.setCursor(40, 0);
    display.println("PWM"); 
    uint16_t a = pw * 127 / 1023; // // table length
    display.drawLine(x, y, x, h, WHITE);
    display.drawLine(x, h, a, h, WHITE);
    display.drawLine(a, h, a, y, WHITE); 
    display.drawLine(a, y, 127, y, WHITE);
      
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

void drawFMStart(int nt) {
  display.clearDisplay();
  uint16_t y = height -1;
  uint16_t x = 0;
  uint16_t h = 20;
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.println("FM-Start"); 
  y = noOnY+LF;
  display.setCursor(ntOnX,y);
  // clear old text
  display.fillRect(ntOnX, y, ntW*4, LF, 0);
  display.println(nt);
  //display.display();
}
void drawFMEnd(int nt) {
  display.clearDisplay();
  uint16_t y = height -1;
  uint16_t x = 0;
  uint16_t h = 20;
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.println("FM-End"); 
  y = noOnY+LF;
  display.setCursor(ntOnX,y);
  // clear old text
  display.fillRect(ntOnX, y, ntW*4, LF, 0);
  display.println(nt);
  //display.display();
}

void drawFMDecay(int nt) {
  display.clearDisplay();
  uint16_t y = height -1;
  uint16_t x = 0;
  uint16_t h = 20;
  display.setTextSize(2);
  display.setCursor(10, 0);
  display.println("FM-Decay"); 
  y = noOnY+LF;
  display.setCursor(ntOnX,y);
  // clear old text
  display.fillRect(ntOnX, y, ntW*4, LF, 0);
  display.println(nt);
  //display.display();
}

void drawLeftArrow() {
  // Navigation arrows
  int x = 0;
  int y = height / 2;
  int h = ARROW_WIDTH;
  int w = 2*h;
  display.fillRect(x, y - h, h+2, w+2, WHITE);
  display.drawLine(x, y, x + h , y - h, BLACK);
  display.drawLine(x, y, x + h , y + h, BLACK);
}

void drawRightArrow() {
  // Navigation arrows
  int h = ARROW_WIDTH;
  int x = width - 1;
  int y = height / 2;
  int w = 2*h;
  display.fillRect(x -h -1, y -h, h+2, w+2, WHITE);
  display.drawLine(x, y, x - h , y - h, BLACK);
  display.drawLine(x, y, x - h , y + h, BLACK);
}

void displayPage0(int mCh, int sNo) {
  display.clearDisplay();
  
  int h = 22;
  int off = 15;
  // text display tests
  display.setTextSize(2);
  int y = 2;
  int x = off;
  display.setCursor(x, y);
  display.println("F&S V1.1"); 
  /*y += h;
  x = 0;
  display.setCursor(x, y);
  display.println("Synth V1.0");
  */
  y += h;
  x = off;
  display.setCursor(x, y);
  // clear old text
  //display.fillRect(x, y, width, h, 0);
  display.print("MidiCh:");
  display.println(mCh);
  // Navigation arrows
  drawLeftArrow();
  drawRightArrow();
  y += h;
  x = off;
 // Waveform:
  display.setCursor(x, y);
  display.print("Wave:");
  if (sNo == 0)
    display.println("Saw");
  else if (sNo == 1)
    display.println("Sin");
  else if (sNo == 2)
    display.println("Tri");  
  else if (sNo == 3)
    display.println("SQR"); 
  else if (sNo == 4)
    display.println("FM"); 
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
    
    display.drawLine(xstart, 0, pwmx, 0, WHITE);
    display.drawLine(pwmx, 0, pwmx, h, WHITE);
    display.drawLine(pwmx, h, xstart + WAVEDISP_WIDTH, h, WHITE);
  }
  else if (w == FM) {
    dispFM();
  }
  else {
    for (int n = 0; n < WAVEDISP_WIDTH; n++)
      display.drawPixel(xstart+n, h - waveFormD[n], WHITE);
  }
}
