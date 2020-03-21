
/**
 * Init ADSR default values to default values
 */
void initAdsrVals() {
  //Serial.println("Set ADSR values");
    setDelayTime(4000); // 
    revAmount = 4000;
    setAttackScale(20);
    setAtValue(20);
    setDecayScale(600);
    setDeValue(600);
    setSustainValue(4095);
    setSusValue(4095);
    setReleaseScale(240);
    setReValue(240);
}

/**
 * change the state of the adsr
 */
void setADSR_mode(int16_t m, int16_t channel) {
  vAdsr[channel].ADSR_mode = m;
}
/**
 * Initialize adsr
 */
void initADSR() {
  for (int16_t n = 0; n < MAXVOICE; n++) {
	  setADSR_mode(STOP, n);
    vAdsr[n].output = 0;
  }
  // Init ADSR Value sets

    adsrValues.A_Step = 0;
    adsrValues.D_Step = 0;
    adsrValues.sustainVal = 0;
    adsrValues.R_Step = 0;
    
   VirtualTableIndexMax = (LogLength << 10) - 1; 
}

/**
 * Handle the gate-on signal (start attack mode)
 */
void setGateOn(int16_t channel) {
  vAdsr[channel].ATableIndex = 0;   // reset table counter 
  vAdsr[MAXVOICE].ATableIndex = 0;  // reset table counter for filter adsr
  setADSR_mode(ATTACK, channel);    // start attack
  setADSR_mode(ATTACK, filter);     // just ad mode!
  
}

/**
 * handle the gate-off signal (start release mode)
 */
void setGateOff(int16_t channel) {
  vAdsr[channel].lastVal = vAdsr[channel].output; // the initial amplitude value for the release mode
  vAdsr[channel].ATableIndex = 0;
  setADSR_mode(RELEASE, channel); // start release
}

/**
 * Calculate the step with which we travel in the adsr tables
 * Analog value goes from 0-4095
 * The log-table goes from 0-4095, the values in there from 0-255
 */
uint16_t calculateScale(uint16_t scaleValue, uint16_t time) {
  unsigned long z = (LogLength * LogLength) / scaleValue; 
  z = z << 10; // *1024 to increase precision of table pointer calculation
  unsigned long n = time * pwmRate;
  z /= n;
  return (uint16_t) z;
}

/**
 * Set attack rate (0-4sec)
 * The table goes from 0 to 4096, attack value determines
 * how fast we travel trough the table.
 * In the end we always should reach the table-end (4096)
 * The Formula for Increments/ISRTick is:
 * Increment/Tick  z = Tablelength^2 / (ScaleValue * TableTime * IRSRate)
 * A scale of 0 = instant attack, 4096 = longest Attack Time
 * @param scaleValue the new attack value
 * @param channel voices or filter setting
 */
void setAttackScale(uint16_t scaleValue) {

  if (scaleValue <= 0)
    scaleValue = 10;
    
  adsrValues.A_Step = calculateScale(scaleValue, ATTACKTIME);
}

/**
 * Set attack rate (0-4sec)
 * @param f the new deacy value
 * @param channel voices or filter setting
 */
void setDecayScale(uint16_t scaleValue) {
  if (scaleValue <= 0)
    scaleValue = 10;
  adsrValues.D_Step = calculateScale(scaleValue, DECAYTIME);
}

/**
 * Set attack rate (0-4sec)
 * @param f the new release value
 * @param channel voices or filter setting
 */
void setReleaseScale(uint16_t scaleValue) {
  if (scaleValue <= 0)
    scaleValue = 10;
  adsrValues.R_Step = calculateScale(scaleValue, RELEASETIME);
}

/**
 * Set Sustain Value
 * @param f the new release
 * @param channel voices or filter setting
 */
void setSustainValue(uint16_t f) {
  adsrValues.sustainVal = f;
}

uint16_t getTableIndex(int16_t channel) {
  return uint16_t (vAdsr[channel].ATableIndex >> 10); // remember: the tableIndex was multiplied by 1024
}

/**
 * read the actual attack value
 * @param channel index of channel
 */
 uint16_t getAttackVal(int16_t channel) {
    return getAttack(getTableIndex(channel));
 }
 
 uint16_t getDecayVal(int16_t channel) {
    return getDecay(getTableIndex(channel));
 }

/**
 * Increase active ADSR-pointers
 * and get the acutal active adsr output value
 * This is called 40000 times a second
 * make channel=filter only do AD, not SR
 * @return the new adsr output value
 */
void addADSRStep(int n) {
  switch (vAdsr[n].ADSR_mode) {
    case ATTACK:
        vAdsr[n].ATableIndex += adsrValues.A_Step; 
        if ( vAdsr[n].ATableIndex >= VirtualTableIndexMax) {
          // end of attack
 
          setADSR_mode(DECAY, n); // switch to decay mode, stop for test
          vAdsr[n].ATableIndex = 0;
        }
        else {
          vAdsr[n].output = getAttackVal(n);

        }
    break;
    case DECAY:
        vAdsr[n].ATableIndex += adsrValues.D_Step; 
        if ( vAdsr[n].ATableIndex >= VirtualTableIndexMax) {
          // end of Decay
          

         // Serial.println(mil);
          setADSR_mode(SUSTAIN, n); // switch to Sustain mode
          vAdsr[n].ATableIndex = 0;
        }
        else {
          // Table goes from 4096 to 0
          vAdsr[n].output = getDecayVal(n);
          //Serial.print("Decay val=");
          //Serial.println( vAdsr[n].output);
          if ( vAdsr[n].output <= adsrValues.sustainVal) {
            

            // Serial.println(mil);
             setADSR_mode(SUSTAIN, n); // switch to Sustain mode
             vAdsr[n].ATableIndex = 0;
          }
        }
    break;
    case SUSTAIN:
      vAdsr[n].output = adsrValues.sustainVal;
    break;
    case RELEASE:
        vAdsr[n].ATableIndex += adsrValues.R_Step; 
        if ( vAdsr[n].ATableIndex >= VirtualTableIndexMax) {
          // end of Release
          vAdsr[n].ATableIndex = 0;
          vAdsr[n].ADSR_mode = STOPPING; // switch off
          vAdsr[n].output = 0;

        }
        else {
          // Table goes from 4096 to 0, sustain value is the starting point
          uint16_t v = getDecayVal(n);   // table value (4096-0)
          v = v * vAdsr[n].lastVal / LogLength;    // weighted with the sustain value
          if ( v <= 1){
            // end of Release

            vAdsr[n].ADSR_mode = STOPPING; // switch off
            vAdsr[n].ATableIndex = 0;
            v = 0;
            //digitalWrite(PC13, 0); // turn on
          }
         /* Serial.print("last= ");
          Serial.print(vAdsr[n].lastVal);
          Serial.print(" Re ");
          Serial.println(v);
          */
          vAdsr[n].output = v;
        }
    break;
  }

}
   
