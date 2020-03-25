

void setGateOn(uint32_t n) {
  voices[n].adsr.counter = 0;
  voices[n].adsr.state = ADSR_ATTACK;
}


void setGateOff(uint32_t n) {
  voices[n].adsr.lastOutput = voices[n].adsr.output;
  voices[n].adsr.counter = 0;
  voices[n].adsr.state = ADSR_RELEASE;
}


void setAttackScale(uint16_t scaleValue) {

  if (scaleValue == 0)
  {
    AmaxCount = 0;
    return;
  } 

  float f = float(SAMPLERATE) * (float(scaleValue) * float(ADSR_MAXATTACKTIME) / float(MIDICONTROLMAXVAL));
  AmaxCount = int(f);
    
}


void setDecayScale(uint16_t scaleValue) {

  if (scaleValue == 0)
  {
    DmaxCount = 0;
    return;
  } 

  float f = float(SAMPLERATE) * (float(scaleValue) * float(ADSR_MAXDECAYTIME) / float(MIDICONTROLMAXVAL));
  DmaxCount = int(f);
}


void setReleaseScale(uint16_t scaleValue) {
  
  if (scaleValue == 0)
  {
    RmaxCount = 0;
    return;
  } 

  float f = float(SAMPLERATE) * (float(scaleValue) * float(ADSR_MAXRELEASETIME) / float(MIDICONTROLMAXVAL));
  RmaxCount = int(f);
}



void addADSRStep(int n) {
  switch (voices[n].adsr.state) {
    case ADSR_ATTACK:
        voices[n].adsr.counter += 1; 
        if ( voices[n].adsr.counter >= AmaxCount) {
          // end of attack
 
          voices[n].adsr.state = ADSR_DECAY;
          voices[n].adsr.counter = 0;
        }
        else {
          uint32_t stepDiv4096 = AmaxCount >> 12;
          if (stepDiv4096 == 0)
            stepDiv4096 = 1;
          uint32_t index = voices[n].adsr.counter / stepDiv4096;
          if (index > 4095)
            index = 4095;
          
          voices[n].adsr.output = getAttack(index);

        }
    break;
    case ADSR_DECAY:
        voices[n].adsr.counter += 1; 
        if ( voices[n].adsr.counter >= DmaxCount) {
 
          voices[n].adsr.state = ADSR_SUSTAIN;
          voices[n].adsr.counter = 0;
        }
        else {

          uint32_t stepDiv4096 = DmaxCount >> 12;
          if (stepDiv4096 == 0)
            stepDiv4096 = 1;
          uint32_t index = voices[n].adsr.counter / stepDiv4096;
          if (index > 4095)
            index = 4095;
          
          voices[n].adsr.output = getDecay(index);


          // TODO this should actually be weighted with the sustain value to always get the same length
          //  (see ADSR_RELEASE for an example)
          if (voices[n].adsr.output <= Sval) {
            
             voices[n].adsr.state = ADSR_SUSTAIN;
             voices[n].adsr.counter = 0;
          }
        }
    break;
    case ADSR_SUSTAIN:
      voices[n].adsr.output = Sval;
    break;
    case ADSR_RELEASE:
        voices[n].adsr.counter += 1; 
        if ( voices[n].adsr.counter >= RmaxCount) {
       
          voices[n].adsr.counter = 0;
          voices[n].adsr.state = ADSR_IDLE;
          voices[n].adsr.output = 0;

        }
        else {

          uint32_t stepDiv4096 = RmaxCount >> 12;
          if (stepDiv4096 == 0)
            stepDiv4096 = 1;
          uint32_t index = voices[n].adsr.counter / stepDiv4096;
          if (index > 4095)
            index = 4095;
          
          uint32_t v = getDecay(index);
          
          
          v = (v * voices[n].adsr.lastOutput) >> 12;
          voices[n].adsr.output = v;
        }
    break;
  }

}
   
