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
float clkInterval = 62500.0; // Default is 120bpm in quavers, as microseconds
uint32_t clkI; // int conversion of the above, probably non-critical but faster to process
const uint16_t ADCLimit = 900; // Approximate upper voltage in 10bit received by ADC

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

      // If the button/trigger was released/went low, change active state:
      // This is because the high state holds the CD4024 in reset, so it potentially can go out of sync
      if(!runStop) active = !active;
    }
  }
}

void clockTime() {
  static uint32_t clkTimer = micros(); // main clock state timer (this is half a step)
  uint16_t clkMod = analogRead(clkModPin); // Read from the ADC (potentiometer+CV)
  static uint16_t l_clkMod; // Used for pot noise reduction
  static bool clkState; // 0 low clock, 1 high clock;

  // Reduce noise from the ADC by only registering changes greater than 3.
  if(clkMod > (l_clkMod+3) || clkMod < (l_clkMod-3)) {
    l_clkMod = clkMod;
    // Map the ADC reading to a range to scale the clock frequency
    int16_t cfshift = map(clkMod, 0, ADCLimit, 40, -30); 
    // Use anti-log function to adjust clock, centre frequency is 120bpm and the clock
    // generates quavers and divides down, therefore cf = 62500µS. Range is approx.
    // 15bpm - 480bpm
    // n.b 20 'steps' of the scale should half or double the frequency
    clkInterval = 62500*pow(1.03526, cfshift); 
    // Worth converting to an int here so float math isn't used constantly by clock
    clkI = clkInterval;
  } 

  if((micros()-clkTimer) >= (clkI/2)) {
    clkTimer = micros();
    clkState = !clkState;

    digitalWrite(clockPin, clkState);
  }
}

// Generate Gate Patterns over 4 beats duration
// Trigger mode sets the pin low every other semiquaver
// Gate mode sets variable gate length pattern
// Trigger is default on reset
void pattern() {
  static bool pattern[32];
  static uint32_t pttnTimer = micros();
  bool pttBtn = digitalRead(pttrnSelPin); // Pattern button boolean 
  static bool l_pttBtn = 0; // transition for pattern button
  static uint32_t btnTimer = millis(); // Debounce timer
  static uint32_t pttnStep = 0; // counter to iterate pattern step
  static uint32_t holdtime = 0; // Timer to check for click or hold
  static bool pttnMode = 0; // 0 for gate mode, 1 for trigger mode
  static bool pttnFlipflop = 0; // Soft flip flop for trigger mode

  // If button pressed, generate a new pattern sequence 
  // Check for button pin transition if debounce timer expires:
  if((millis()-btnTimer) >= 50) {
    if(pttBtn != l_pttBtn) {
      l_pttBtn = pttBtn;
      btnTimer = millis();

      // If the button was pressed:
      if(!pttBtn) {
        holdtime = millis();
        // writes a 32 item array with random bits
        for(uint8_t n = 0; n < 32; n++) {
          pattern[n] = random(2); // Write a bit (or not) into pattern[n]
        }
      }
      // If the button was released:
      else {
        // Long press means switch mode:
        if((millis()-holdtime) > 1000) {
          pttnMode = !pttnMode;
        }
      }
    }
  }

  // Iterate through the pattern:
  // Gate mode:
  if(!pttnMode) {
    if((micros()-pttnTimer) >= (clkI/2)) {
      pttnTimer = micros();
      
      digitalWrite(pttrnOutPin, pattern[pttnStep]);
      pttnStep++;
      if(pttnStep >= 32) pttnStep = 0;
    }
  }
  // Trigger mode:
  else {
    if((micros()-pttnTimer) >= (clkI/4)) {
      pttnTimer = micros();

      if(pttnFlipflop) {
        digitalWrite(pttrnOutPin, pattern[pttnStep]);
        pttnStep++;
        if(pttnStep >= 32) pttnStep = 0;
      }
      else {
        digitalWrite(pttrnOutPin, LOW);
      }
      pttnFlipflop = !pttnFlipflop;
    }
  }
}
