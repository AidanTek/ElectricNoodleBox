/*
TimeSeeds Firmware for ATTiny85

Copyright (C) 2020 Aidan Taylor

This project is released under the CERN-OHL-W license, please see
https://github.com/AidanTek/ElectricNoodleBox/blob/master/LICENSE
for details. 

The firmware has the requisite of the ATTinyCore by SpenceKonde,
which can be found here: https://github.com/SpenceKonde/ATTinyCore

ATTinyCore is unmodified in this instance and released under the 
GNU Lesser GPL license as described below:

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// Hardware pins on ATTiny85:
uint8_t runStopPin = 4; // 3 Run/Stop the clock
uint8_t pttrnSelPin = 5; // 4 Pattern Select
uint8_t clkModPin = A0; // 2 ADC input for clock modulation
uint8_t mstClkPin = 2; // 1 Master clock output
uint8_t pttrnOutPin = 3; // 0 Pattern Output

// Global software variables:
bool runstop = 0; // 0 = stop, 1 = run
bool softClkState = 0; // software clock state
bool halfClk = 0; // master clock is every other soft clock
bool mstClkState = 1; // master clock n.b. this is inverted in hardware
bool nPattern = 1; // Flag to generate new pattern
uint8_t pttrnPos = 0; // Pattern step 1 of 16 
bool pttrnAdv = 0; // soft clock advance pattern step

void setup() {
  pinMode(runStopPin, INPUT); // R28 will pull input LOW
  pinMode(pttrnSelPin, INPUT_PULLUP);
  pinMode(mstClkPin, OUTPUT);
  pinMode(pttrnOutPin, OUTPUT);
}

void loop() {
  runStop();
  patternSelect();
  runstop = 1; // for testing
  // If the clock is active:
  if(runstop) {
    softwareClock();
    if(softClkState) {
      halfClk = !halfClk;
      // When halfClk = 1 (every other soft clock);
      if(halfClk) {
        mstClkState = !mstClkState;
        digitalWrite(mstClkPin, mstClkState);
      }
    }
    //patternClock();
  }
}

bool runStop() {
  // Variable and transition to detect pulse on Run/Stop pin
  bool active = digitalRead(runStopPin);
  static bool l_active = 0;
  static uint32_t rsDebounceMS = millis();

  // 50ms debounce to avoid button repeat readings
  if((millis() - rsDebounceMS) > 50) {
    // On transition:
    if(active != l_active) {
      l_active = active;
      // Debounce timer reset
      rsDebounceMS = millis();
      
      // If pin went HIGH then flip logic of runstop variable:
      if(active) {
        runstop = !runstop;
        softClkState = 0;
        mstClkState = 1;
      }
    }
  }
}

void softwareClock() {
  static uint32_t softClkMs = 0; // software clock interval timer
  
  if((millis() - softClkMs) > 1000) {
    softClkMs = millis();
    softClkState = !softClkState;
    if(softClkState) {
      pttrnAdv = 1;
    }
  }
}

void patternSelect() {
  // Variable and transition to detect button press on pattern pin
  bool pttrnState = digitalRead(pttrnSelPin);
  static bool l_pttrnState = 0;
  static uint32_t psDebounceMS = millis();

  // 50ms debounce to avoid button repeat readings
  if((millis() - psDebounceMS) > 50) {
    // On transition:
    if(pttrnState != l_pttrnState) {
      l_pttrnState = pttrnState;
      // Debounce timer reset
      psDebounceMS = millis();
      
      // If pin went HIGH then set new pattern flag:
      if(pttrnState) {
        nPattern = 1;
      }
    }
  }
}

void patternClock() {
  static bool pattern[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  
  // If new pattern flag set:
  if(nPattern) {
    nPattern = 0;
    for(uint8_t n = 0; n < 16; n++) {
      pattern[n] = random(2);
    }
  }

  if(pttrnAdv) {
    pttrnAdv = 0;
    digitalWrite(pttrnOutPin, pattern[pttrnPos]);
    pttrnPos++;
    if(pttrnPos == 16) pttrnPos = 0;
  }
}
