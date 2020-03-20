

void initControls() {


  for(int n = 0; n < noOfButtons; n++) {
    buttons[n].debounceDelay = 100;
    buttons[n].lastDebounceTime = 0;
    buttons[n].lastButtonState = 1;             // not pressed
    //pinMode(buttons[n].portPin, INPUT_PULLUP);
  }

  

  for (int n = 0; n < MAXPOT; n++) {
     mcount[n] = 0;
     avgsum[n] = 0;
     Avalue[n] = 0;
     //pinMode(pin[n], INPUT_ANALOG);
  }

  //pinMode(pinB,INPUT_ANALOG);

}



/**
 * read an analog channel. Store value in Avalue[potCh]
 * @param potCh the analoge channel to read
 * @return true if the value was changed
 */
bool updateAnalogChannel(uint16_t potCh) {

  /*
  if (potCh != 5)
  {
   // Read the Potentiometer value 
   bool c = false;
 
   int Nscale = adc_read(ADC2, pin[potCh]);       // ADC2 read noisy voltage value from pot.
   //Serial.print(" Nscale=");
   //Serial.print(Nscale);
   uint32 m = glidingAverage(  Nscale, potCh );   // Add the new adc value into a gliding average value
   //Serial.print(" GAverage=");
   //Serial.print(m);
   uint32 d = abs(m - Avalue[potCh]);
   //Serial.print(" diff=");
   //Serial.println(d);
   if (d > 5) {
      Avalue[potCh] = m;
      c = true;
   }
   return c;
  }
  */
  return false;
}
// ----------- switches amd buttons: -------------------------
/**
 * @param btn number of the button/switch
 * @return true if changed
 */
boolean readInputPin(uint32_t mil, int btn) {
  /*
  // Reset Button:
    bool res = false;
    uint16_t bt = digitalRead(buttons[btn].portPin); // 0=clear, 1 = neutral
   // Serial.println(bt);
    uint32_t diff = mil - buttons[btn].lastDebounceTime;
    //  Serial.println(diff);
    if (buttons[btn].lastButtonState != bt && diff > buttons[btn].debounceDelay ) {
      if (bt == LOW) {
        // pressed
          buttons[btn].lastDebounceTime = mil;
        }
    }
    res = buttons[btn].lastButtonState != bt;
    buttons[btn].lastButtonState = bt; // here we save the actual button state

    return res;
    */
    return false;
}

bool readEncoder(uint32_t mil) {


  /*
     int inc = 1;
     aVal = digitalRead(pinA);
    // int bVal = digitalRead(pinB);
     bool change = false;
     unsigned long diff = mil - TimeOfLastDebounce;
     if (aVal != pinALast && diff > 50){ 
      // Means the knob is rotating
      //print("inc=", inc);
       bCW = digitalRead(pinB) != aVal;
      (bCW)?encoderPosCount -=inc: encoderPosCount +=inc;
      TimeOfLastDebounce = mil; 
      change = true;
     }
     pinALast = aVal;

     return change;*/


/*
    // Read the Potentiometer value 
   bool c = false;

   int Nscale = adc_read(ADC2, pinB);       // ADC2 read noisy voltage value from pot.
   //Serial.print(" Nscale=");
   //Serial.print(Nscale);
   uint32 aVal = glidingAverage(  Nscale, 5 );   // Add the new adc value into a gliding average value
   //Serial.print(" GAverage=");
   //Serial.print(m);
   uint32 d = abs(aVal - Avalue[5]);
   //Serial.print(" diff=");
   //Serial.println(d);
   if (d > 5) {
      Avalue[5] = aVal;
      c = true;
   }
   
      //if (aVal > 255) aVal = 255;
      //aVal = map(aVal, 0, 255, 0, 16);
      Serial.println(aVal);
     
     if (aVal != pinALast){ 
      Serial.print(aVal);
      Serial.print(" ");
      Serial.println(pinALast);
      bCW = aVal > pinALast;
     (bCW)?encoderPosCount -=1: encoderPosCount +=1;
 
     }

     
     pinALast = aVal;
     
     return c;*/
     return false;
 }
 
