// Hardware pin reference - THESE NEED UPDATING
const uint8_t encPin[3] = {1,2,14}; // encoded output, MCU pins 19,18,17
const uint8_t decSelPin = 10; // Decoder select, MCU pin 4 
const uint8_t extClkPin = 9; // external clock in
const uint8_t intClkPin = 0; // Internal clock mod
const uint8_t seqLenPin = 5; // Sequence length mod
const uint8_t dirPin = 3; // Sequence direction select
const uint8_t randomPin = 4; // Random modifier 
const uint8_t gateInPin = 11; // Gate "active" input - link pin to 5V if unused
const uint8_t gateOutPin = 7; // Gate output
const uint8_t runStopPin = 8; // Sequence start/stop
const uint8_t seqResetPin = 12; // Sequence reset
const uint8_t gateWidthPin = 13; // Gate width modifier

// Software global variables
  // Internal / External clock
bool clksrc = 0; // 0 internal, 1 external
uint16_t clkmod = 250; // Internal clock time interval
bool stepAdv = 0; // When either clock goes 'high' this value is 1
bool active = 0; // Sequence run/stop toggle
bool seqReset = 0; // Toggle a reset
uint32_t gateStartMS = 0; // Gate start time sample
bool gateOn = 0; // 0 = gate inactive, 1 = gate active

// Sequence steps encoded as 3-bit:
const bool enc[8][3] = {
  {0,0,0},
  {1,0,0},
  {0,1,0},
  {1,1,0},
  {0,0,1},
  {1,0,1},
  {0,1,1},
  {1,1,1}
};

void setup() {
  // Setup hardware pins
  for(uint8_t n = 0; n < 3; n++) {
    pinMode(encPin[n], OUTPUT);
  }
  pinMode(decSelPin, OUTPUT);
  pinMode(extClkPin, INPUT);
  pinMode(dirPin, INPUT);
  pinMode(gateInPin, INPUT);
  pinMode(gateOutPin, OUTPUT);
  pinMode(runStopPin, INPUT);
  pinMode(seqResetPin, INPUT);
}

void loop() {
  runStopReset();

  if(active) {
    clockupdate(); // See below
    
    // Update the sequence on new clock step:
    if(stepAdv) {
      stepAdv = 0;
      sequenceModifiers();
      gateopen();
    }
  }
  gateclose();
}

/* This function checks run/stop pin and reset pin run/stop activates and
deactivates the sequence by setting a global variable. Reset sets a global flag 
which will be detected in seqModifiers() and return to step 0 (or last step 
depending on direction and length). */
void runStopReset() {
  // Read from the sequence run pin, with static variable for transition
  bool runStop = digitalRead(runStopPin);
  static bool l_runStop = 0;

  // Check for reset, with static variable for transition
  bool res = digitalRead(seqResetPin);
  static bool l_res = 0;

  // On runStop transition:
  if(runStop != l_runStop) {
    l_runStop = runStop;

    // If runStop change detected, flip active state:
    if(runStop) {
      active = !active;
    }
  }

  // On reset transition:
  if(res != l_res) {
    l_res = res;

    // If reset pin detects rising edge, set reset flag
    if(res) seqReset = 1;
  }
}

/* Clock default is internal clock but a rising pulse on the external clock 
input will cancel it and sequence steps will then follow external pulse. Adjust 
the clock mod pot to reinstate the internal clock. */
void clockupdate() {
  // static variable, for internal clock transition 
  static uint16_t l_intclk = analogRead(intClkPin);
  
  // Look for a high pulse on the external clock pin
  bool ext = digitalRead(extClkPin);
  if(ext) clksrc = 1;

  // Otherwise, detect change on internal clock pin
  uint16_t intclk = analogRead(intClkPin);
  // Transition threshold of 10 to counter noise
  if(intclk <= (l_intclk-10) || intclk >= (l_intclk+10)) clksrc = 0;

  // If externally clocked:
  if(clksrc) {
    bool extclk = digitalRead(extClkPin);
    // A static variable for clock transition
    static bool l_extclk = 0;

    if(extclk != l_extclk) {
      // Update transition
      l_extclk = extclk;
      // Update step if pin went high:
      if(extclk) stepAdv = 1;
    }
  }
  // If internally clocked:
  else {
    // Internal clock plays quarter notes from 7.5 to 240bpm
    // clkmod is now a global variable, so it can be accessed by gateClose()
    clkmod = map(analogRead(intClkPin), 0, 1023, 2000, 62);
  
    static uint32_t iclktimer = millis();
    if((millis() - iclktimer) >= clkmod) {
      iclktimer = millis();
      stepAdv = 1;
    }
  }
}

void bitencode(uint8_t seqstep) {
  if(seqstep < 8) {
    digitalWrite(decSelPin, LOW);
    for(uint8_t n = 0; n < 3; n++) {
      digitalWrite(encPin[n],enc[seqstep][n]);
    }
  }
  else {
    digitalWrite(decSelPin, HIGH);
    for(uint8_t n = 0; n < 3; n++) {
      digitalWrite(encPin[n],enc[seqstep-8][n]);
    }
  }
}

/* Sequence step position iteration will depend on a number of modifiers: 
direction, length and random. Random is complex, the single control has four 
states: off, stutter, pattern and pseudorandom. Stutter will increase the 
likelyhood that a note is repeated, pattern will repeatedly step through a 
randomly generated pattern (dependent on length setting) and pseudorandom will 
pick a random-seeded number every step. This function will write to the encoders
before before iterating the sequence and exiting in each case. */
uint8_t sequenceModifiers() {
  bool dir = digitalRead(dirPin); // 0 for forwards, 1 for backwards
  uint16_t seqLength = map(analogRead(seqLenPin), 0, 1023, 0, 15);
  uint16_t randMod = map(analogRead(randomPin), 0, 1023, 0, 7);
  // A transistion is needed for the random pattern change:
  static uint8_t l_randMod = 0;
  // And a static array records the pattern:
  static uint8_t pattern[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  // A static variable is needed to remember stutters
  static uint8_t stutter = 0;
  // A static variable to keep sequence position
  static uint8_t posN = 0;

  // If the reset flag is set to 1, then restart the sequence
  if(seqReset) {
    if(dir) posN = 0;
    else posN = seqLength;
    // Set the reset flag back to 0
    seqReset = 0;
  }

  // The random modifier determines the sequence behaviour. The Random pot range
  // is divided into 8 segments, 0: Off, 1-3: Stutter 16%-64%, 4-6: Patterns,
  // and 7: Pseudorandom. 
  // First segment (off) is standard sequencing:
  if(randMod == 0) {
    if(dir) {
      bitencode(posN);
      posN++;
      if(posN > seqLength) posN = 0;
    }
    else {
      bitencode(posN);
      posN--;
      // Note, subtracting 1 from 0 will result in 255
      if(posN == 0 || posN == 255) posN = seqLength; 
    }
    return;
  }
  
  // Next segment injects stutter:
  else if(randMod < 4) {
    // random chance, if 'true' then the last stored value in stutter is 
    // returned, if 'false' then the sequence catches up to actual position and 
    // stutter is replaced with step n. This means that the same note can be 
    // repeated several times: 
    uint8_t r = random(100);
    // 16 is multiplied by 1 to 3, for a 1:6, a 1:3 or a 2:3 chance of stutter
    if(r <= (16*(randMod-1))) {
      if(dir) {
        bitencode(stutter);
        posN++; // iterate the sequence
        if(posN > seqLength) posN = 0;
      }
      else {
        bitencode(stutter);
        posN--; // iterate the sequence
        if(posN == 0 || posN == 255) posN = seqLength;
      }
      return; // return the previous active sequence step
    }
    else {
      if(dir) {
        bitencode(posN);
        posN++; // iterate the sequence
        if(posN > seqLength) posN = 0;
      }
      else {
        bitencode(posN);
        posN--; // iterate the sequence
        if(posN == 0 || posN == 255) posN = seqLength;
      }
      stutter = posN; // update stutter
      return;
    }
  }
  
  // The next segment will generate a random sequence pattern. A new pattern 
  // can be generated by turning the random knob to the next pattern position. 
  // The pattern has 16 steps, playback is subject to the length control:
  else if(randMod < 7) {
    // A transition controls regeneration of pattern array:
    if(randMod != l_randMod) {
      l_randMod = randMod;
      for(uint8_t x = 0; x < 16; x++) {
        pattern[x] = random(16);
      }
    }
    
    // Step through the generated pattern:
    if(dir) {
      bitencode(pattern[posN]);
      posN++;
      if(posN > seqLength) posN = 0;
    }
    else {
      bitencode(pattern[posN]);
      posN--;
      // Note, subtracting 1 from 0 will result in 255
      if(posN == 0 || posN == 255) posN = seqLength;
    }
    return;
  }
  
  // Otherwise, generate a pseudorandom number, subject to length
  else  {
    bitencode(random(seqLength));
  }
}

/* The gate output is subject to the state of the gate 'active' pin. Each 
hardware sequence step has a series switch with a common connection back to 
this pin. If the switch is in the on position then the 'HIGH' pulse will be 
detected by the MCU, and subsequently a gate will be output from the gate 
output pin. The gate has adjustable pulse width which ideally is a ratio of the 
clock frequency, this is useful for analogue envelopes with 'hold' 
functionality. Gate handling will be different for internal and external 
clocking, internal clocking is somewhat simpler, because we know the interval. 
External clocking presents some challenges because the source isn't necessarily 
a consistent pulse. I opted for a set gate time in this case, even if it may be
lost at higher frequencies. You can always use the clock pulse itself if it is
going to change a lot! */

// Gates are only opened on a new sequence step, they are closed in realtime 
// with the function below:
void gateopen() {
  // The sequencer has a switch for each step with a common connection
  // to the MCU, so steps can be active or inactive:
  gateActive = digitalRead(gateInPin);

  // Open the gate if switch in HIGH position:
  if(gateActive) {
    digitalWrite(gateOutPin, HIGH);
    // Set the global variables for gateclose() to pick up
    gateStartMS = millis(); // Update the start timer
    gateOn = 1; // Boolean flag
  }
}

void gateclose() {
  if(gateOn) {
    // Sample from the gateWidth pin, and map to range:
    uint16_t gateWidth = map(analogRead(gateWidthPin), 0, 1023, 10, 90);
    // Internal clocking
    if(!clksrc) {
      // interval conversion, use a float for the operation:
      float gateinterval = (clkmod/100)*gateWidth;
      // Close the gate if the time is up:
      if((millis()-gateStartMS) > gateinterval) {
        digitalWrite(gateOutPin, LOW);
        gateOn = 0;
      }
    }
  }
}
