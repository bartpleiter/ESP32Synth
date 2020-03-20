
void initRev(void) {
  maxR = RevLength - 1;
  delayTime = 0;
  writePointer = 0;
  readPointer = 0;
}


/**
 * Set new delay time. @10000 samples buffer, we have a max. of 250 ms delay (@40kHz)
 * @param t 
 */
void setDelayTime(uint16_t t) {
  changes = true;
  uint32_t z = t * RevLength;
  uint32_t n =  (z >> 12); // 4096 is the adc range for the delay potentiometer
  uint16_t nw = (uint16_t) (n & 0xFFFF);
  int16_t delayDiff = nw - delayTime;
  if (delayDiff < 0) {
	  readPointer -= delayDiff;
	  if (readPointer > maxR) 
			readPointer -= RevLength;
    diff = 0; 
  }
  else {
	  // new time is larger, wait to read until that time is passed
	  diff += delayDiff;
  }
  delayTime = nw;
  //Serial.print("delayTime=");
  //Serial.println(delayTime);
  /*int16_t p = writePointer - delayTime; //( this can be neg. )
  if (p < 0)
    p = RevLength  + p;
*/
  changes = false;
}

/**
 * @param 0-0x3ff 
 */
void writeHead(int8_t v) {
  
  int8_t value = v;
  rev[writePointer++] = value;
  if (writePointer > maxR)
    writePointer = 0; // buffer overflow
  if (diff > 0) {
    diff--; // a new value was written, so decrease diff by one. Once diff is 0, we start to read the reverb value
  }
  /*
  Serial.print("writeHead prtr=");
  Serial.print(writePointer);
  Serial.print(" v=");
  Serial.print(value);
  Serial.print(" diff=");
  Serial.println(diff);
  */
}

int8_t readRev(void) {
  int8_t t = 0;
  if (diff == 0) {
	  t = rev[readPointer++];
	if (readPointer > maxR)
		readPointer = 0; // buffer overflow
  }
  return t;
}
