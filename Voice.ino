void initVoices() 
{
  for (uint32_t n = 0; n < MAXVOICES; n++) 
  {
    voices[n].activationTime  = 0;
    voices[n].indexStep       = 0;
    voices[n].tableIndex      = 0;
    voices[n].adsr.state      = ADSR_IDLE;
    voices[n].adsr.output     = 0;
  }
}


void setVoiceFreqency(uint32_t f, uint32_t n) 
{
  voices[n].indexStep = ((WAVETABLELENGTH  * f) <<8) / SAMPLERATE; // multiply by 256 to prevent losing precision
  voices[n].tableIndex = 0;
}


void addStep(uint32_t n) 
{
  voices[n].tableIndex += voices[n].indexStep;     // step through the wavetable, the larger the step value, the higher the frequency
    
  uint32_t tIdx = (voices[n].tableIndex >> 8) & (WAVETABLELENGTH-1);   // wrap around table index (requires WAVETABLELENGTH to be a power of 2)

  int32_t amplitude = 0;
  
  // find out which table to use:
  if(waveForm == SAW)
    amplitude = getSawInt(tIdx);
  else if(waveForm == SIN)
    amplitude = getSinInt(tIdx);  
  else if(waveForm == TRI)
    amplitude = getTriInt(tIdx); 
  else if(waveForm == SQR)
    amplitude = (tIdx < pwmWidth)?-2047:2047; 
    
  voices[n].output = amplitude; //  +- 2047
}
 
