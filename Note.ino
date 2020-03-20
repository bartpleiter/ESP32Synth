/**
 * Struct to define one Midi note
 */
 typedef struct {
  byte note;          // WHICH NOTE
  byte velocity;      // how loud
  uint16_t duration;  // how long
  uint32_t startTime; // when to be started
  bool active;        // true if activly playing
  byte slot;          // which buffer slot this note got
 } Note_type;
 


