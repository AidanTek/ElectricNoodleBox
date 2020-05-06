// Hardware pin reference
const uint8_t encPin[3] = {1,2,14}; // encoded output, MCU pins 19,18,17
const uint8_t decSelPin = 6; // Decoder select, MCU pin 4 

// Encoded sequence
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

}

void loop() {
  // Stand in counter:
  static byte c;
//  if(c >= 16) c = 0;
  c = random(16);
  bitencode(c);
  c++;
  delay(50);
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
