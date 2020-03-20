/**
 * Describes the voices
 */



/**
 * Method declaration
 */
void getWaveAmp(uint16_t);
void selectWave(int16_t);


void initVoices() {
  for (int16_t n = 0; n < MAXVOICE; n++) {
    voice[n].activationTime = 0;
    voice[n].step = 0;
    voice[n].tableIndexEnv3 = 0;
  }
  initADSR();
}

/**
 * Select the actual playing Waveform
 */
void selectWave(int16_t w) {
    waveForm = w;
    tableLength = 2048L;
    mTableIndexMax = (tableLength << 8) - 1; 
}

/**
 * Just calculate the step for the given frrequency
 * @param f the new frequency
 * @param slot, the slot in the voice array to use
 * @param mil the actual time-stamp in ms
 */
void setVoiceFreqency(uint16_t f, int16_t slot, uint32_t mil) {
  //Serial.print("Start note=");
  //Serial.print(note); 
  voice[slot].activationTime = mil;
 
 /* Serial.print(" freq=");
  Serial.print(f);
  */
  unsigned long z = tableLength  * (unsigned long) f;       // multiply table-length by target frequency
  z = z << 8;                                               // *256 to increase precision of table pointer calculation
  z = z / ISR_RATE;
  //Serial.print(" z=");
  //Serial.println(z);
  voice[slot].step = (uint16_t) z;
  
  /*Serial.print(" step=");
  Serial.println(voice[slot].step);
  */
}

/**
 * Just calculate the step for the given frrequency
 * @param f the new frequency
 * @param slot, the slot in the voice array to use
 * @param mil the actual time-stamp in ms
 */
void setFMFrequencys(uint16_t f, int16_t slot, uint32_t mil) {
  //Serial.print("Start note=");
  //Serial.print(note); 

   /* v_start = 384;     
    v_end = 256;
    fm_modulator = 64;
    */
    /*
    Serial.print(" FM freq=");
    Serial.print(f);
    Serial.print(" range=");
    Serial.print(range);
    */
    voice[slot].activationTime = mil;
   
    uint64_t z = SinusLengthInt  * (uint64_t) f; //Oszil1, multiply table-length by target frequency
    z = z << precisionShift;                    // *4096 to increase precision of table pointer calculation
    voice[slot].step = z / ISR_RATE;           // use this to step through the sine-table
    //Serial.print(" step=");
    //Serial.println(voice[slot].step);
  
    /*uint64_t z = ((unsigned long) f) * fm_tableLength; //range;
    z = z << precisionShift;
    Serial.print(" z=");
    Serial.println(z);
    //z = z / divider; // divider = 8;
    Serial.print(" z1=");
    Serial.println(z);
    z = z / ISR_RATE;
    Serial.print(" z2=");
    Serial.println(z);
    voice[slot].step = z;
    */
    /*Serial.print(" step=");
    Serial.print( voice[slot].step);
    
    Serial.print(" fm_modulator=");
    Serial.print(fm_modulator);
    */
    voice[slot].modulatorStep = (voice[slot].step * fm_modulator ) / 256;
    //Serial.print(" modulatorStep=");
    //Serial.print( voice[slot].modulatorStep);
    voice[slot].volDiff = v_start - v_end;
    //Serial.print(" volDiff=");
    //Serial.println(voice[slot].volDiff);
    voice[slot].tableIndexEnv3 = 0;
    voice[slot].envStep3 = calculateScale(fm_decay, DECAYTIME);
   //Serial.print(" step=");
   //Serial.println(step[n]);
}

void setFMModulator(uint16_t val) {
  fm_modulator = val;
}
/**
 * Increase all active Wavetable-pointers
 * and get the acutal active analog output value
 * @return the new analog output value
 */
void addStep(int n) {
   voice[n].tableIndex += voice[n].step;     // step through the wavetable, the larger the step value, the higher the frequency
  
   int c = 0;
   // here access the tables, divide tableIndex by 256
   uint32_t tIdx = voice[n].tableIndex >> 8;      // remember: the tableIndex was multiplied by 256, so here we divide
   tIdx = tIdx & 0x7ff; // 2048
   
   // find out which table to use:
   if(waveForm == 0)
     c = getSawInt(tIdx);
   else if(waveForm == 1)
     c = getSinInt(tIdx);  
   else if(waveForm == 2)
     c = getTriInt(tIdx); 
   else if(waveForm == 3) {// square, use pwm value!
     int pv = pwm_CValue + pwm_value; 
     if (pv > 1020)
       pv = 1020;
     c = (tIdx < pv)?-2047:2047; 
   }
   voice[n].vOutput = c; //  +- 2047
}


/**
* FM instrument
*/
    void addFMStep(int n) {
        // ------------------------------------ Frequency modulation ---------------------------------
       
     /*Serial.print("FM n=");
      Serial.print( n); 
     */
       // Modulator envelope
       int16_t e3 = 0;
       voice[n].tableIndexEnv3 += voice[n].envStep3;
       if (voice[n].tableIndexEnv3 < VirtualTableIndexMax) { 
         int16_t t_idx = (uint16_t) ( voice[n].tableIndexEnv3 >> precisionShift); 
         e3 = getDecay(t_idx); //envelope 1 (0-4095)
         uint16_t envE3 = (e3 * voice[n].volDiff) >> 12;
         voice[n].fmVol = v_end + envE3;
        //Serial.print("FMVol=");
        //Serial.print(voice[n].fmVol);
       }
       //--------------------------------------------------
       voice[n].tableIndex += voice[n].step;
       voice[n].tableIndex =  voice[n].tableIndex &  0x7fffff;
       //Serial.print(" baseIdx=");
       //Serial.print(voice[n].tableIndex);
       
       voice[n].modTableIndex += voice[n].modulatorStep;
       voice[n].modTableIndex =  voice[n].modTableIndex &  0x7fffff;
       //Serial.print(" modIdx=");
       //Serial.print(voice[n].modTableIndex);
       
       //Serial.print(" ScaleV=");
       //Serial.print(ScaleV);
       uint16_t fmidx = voice[n].modTableIndex >> precisionShift; 
       //Serial.print(" fmidx=");
       //Serial.print(fmidx);
       int16_t modSin = getSinInt(fmidx); // modulator freq +- 127
       //Serial.print(" modSin=");
       //Serial.print(modSin);
      
       //-------------------------------------------
       // modulator envelope:
       int32_t envModSin = divider2 * modSin * voice[n].fmVol; // ScaleV * 
       envModSin = envModSin / 2;
       
       //Serial.print(" envModSin=");
       //Serial.print(envModSin);
       envModSin = envModSin % 0x7fffff;
       //Serial.print(" masked envModSin=");
       //Serial.print(envModSin);
       //Serial.print(" baseIndex=");
       //Serial.print( voice[n].tableIndex);
       
       uint64_t addedPhase = voice[n].tableIndex + envModSin;
       addedPhase = addedPhase & 0x7fffff;
       //Serial.print(" addedPhase=");
       //Serial.print(addedPhase);
       uint16_t bidx = addedPhase >> precisionShift;
       //Serial.print(" baseidx=");
       //Serial.print(bidx);
       //bidx = voice[n].tableIndex >> precisionShift;
       int16_t v = getSinInt(bidx); // 
       //-------------------------- End of sfm oszillator ---------------------
       //Serial.print(" base v=");
       //Serial.println(v);
       
       voice[n].vOutput = v;
     }


 
 
