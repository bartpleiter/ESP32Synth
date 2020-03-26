/*
 * VOICE
 * Contains all voice related functions
 */


// Initialize all voices
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


// Set frequency of voice n by setting the index step based on the given frequency
// Index step is multiplied by 256 to preserve precision
void setVoiceFreqency(uint32_t f, uint32_t n) 
{
  voices[n].indexStep = ((WAVETABLELENGTH  * f) <<8) / SAMPLERATE; // multiply by 256 to prevent losing precision
  voices[n].tableIndex = 0;
}


// Updates the state of the voice by incrementing counters and traversing trough the wave tables
void updateVoice(uint32_t n) 
{
  // step through the wavetable, the larger the step value, the higher the frequency
  voices[n].tableIndex += voices[n].indexStep;  

  // wrap around table index (requires WAVETABLELENGTH to be a power of 2)
  // also note that we divide here by 256, since the index step is multiplied by 256 (for precision)
  uint32_t tIdx = (voices[n].tableIndex >> 8) & (WAVETABLELENGTH-1);   

  int32_t amplitude = 0;
  // which table to use:
  if(waveForm == SAW)
    amplitude = getSawInt(tIdx);
  else if(waveForm == SIN)
    amplitude = getSinInt(tIdx);  
  else if(waveForm == TRI)
    amplitude = getTriInt(tIdx); 
  else if(waveForm == SQR)
    amplitude = (tIdx < pwmWidth)?-2047:2047; 

  // set output value
  voices[n].output = amplitude; //  +- 2047
}
