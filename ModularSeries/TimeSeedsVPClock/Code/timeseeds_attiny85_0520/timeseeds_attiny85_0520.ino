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
const uint8_t runStopPin = 3; // 3 Run/Stop the clock
const uint8_t pttrnSelPin = 4; // 4 Pattern Select
const uint8_t clkModPin = A1; // A1 ADC input for clock modulation
const uint8_t clockPin = 1; // 1 Main Clock output
const uint8_t pttrnOutPin = 0; // 0 Pattern Output

// Global Variables
bool active = 0; // 0 stop, 1 run
uint32_t clkInterval = 83333; // Default is 90bpm in quavers, as microseconds
const uint16_t ADCLimit = 877; // Approximate upper voltage in 10bit received by ADC

void setup() {
  // Hardware Pin Setup
  pinMode(runStopPin, INPUT);
  pinMode(pttrnSelPin, INPUT_PULLUP);
  pinMode(clockPin, OUTPUT);
  pinMode(pttrnOutPin, OUTPUT);
}

void loop() {
  runStop();

  // 'active' controls the rest of the loop:
  if(active) {
    clockTime(); // Main clock modulation and output
    pattern(); // Gate pattern generator and output
  }
}

void runStop() {
  bool runStop = digitalRead(runStopPin); // runStop button boolean 
  static bool l_runStop = 0; // transition for runStop
  static uint32_t rsTimer = millis(); // Debounce timer

  // If debounce has timed out, check if the button state has changed:
  if((millis() - rsTimer) >= 50) {
    if(runStop != l_runStop) {
      l_runStop = runStop;
      rsTimer = millis();

      // If the button was pressed, change active state:
      if(runStop) active = !active;
    }
  }
}

void clockTime() {
  static uint32_t clkTimer = micros(); // main clock state timer (this is half a step)
  uint16_t clkMod = analogRead(clkModPin); // Read from the ADC (potentiometer+CV)
  static uint16_t l_clkMod = 0; // Used for pot noise reduction
  static bool clkState = 0; // 0 low clock, 1 high clock

  // Reduce noise from the ADC by only registering changes greater than 3.
  if(clkMod > (l_clkMod+3) || clkMod < (l_clkMod-3)) {
    l_clkMod = clkMod;
    clkInterval = map(clkMod, 0, ADCLimit, 750000, 7813); 
  }

  if((micros()-clkTimer) >= clkInterval) {
    clkTimer = micros();
    clkState = !clkState;

    digitalWrite(clockPin, clkState);
  }
}

// Generate Gate Patterns over 4 beats 
void pattern() {
  static bool pattern[32];
  static uint32_t pttnTimer = micros();
  bool pttBtn = digitalRead(pttrnSelPin); // Pattern button boolean 
  static bool l_pttBtn = 0; // transition for pattern button
  static uint32_t btnTimer = millis(); // Debounce timer
  static uint32_t pttnStep = 0; // counter to iterate pattern step
 

  // If button pressed, generate a new pattern sequence 
  // Check for button pin transition if debounce timer expires:
  if((millis()-btnTimer) >= 50) {
    if(pttBtn != l_pttBtn) {
      l_pttBtn = pttBtn;
      btnTimer = millis();

      // If the button was pressed
      if(pttBtn) {
        for(uint8_t n = 0; n < 32; n++) {
          pattern[n] = random(2); // Write a bit (or not) into pattern[n]
        }
      }
    }
  }

  // Iterate through the pattern:
  if((micros()-pttnTimer) >= clkInterval) {
    pttnTimer = micros();
    
    digitalWrite(pttrnOutPin, pattern[pttnStep]);
    pttnStep++;
    if(pttnStep >= 32) pttnStep = 0;
  }
}
