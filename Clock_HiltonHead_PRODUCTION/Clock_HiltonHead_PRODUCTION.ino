#include "Wire.h"
#include "SevSeg.h"

SevSeg sevseg;
byte numDigitsIn        = 4;
byte digitPins[]        = {10, 11, 12, A2};
byte segmentPins[]      = {A1, 4, 5, 6, 7, 8, 9, A0};
byte resOnSegmentsIn    = true;
bool updateWithDelaysIn = false;
bool leadingZerosIn     = false;
bool disableDecPoint    = false;

byte decToBcd(byte val) { return ( (val/10*16) + (val%10) ); }
byte bcdToDec(byte val) { return ( (val/16*10) + (val%16) ); }

#define setMinutePin    2
bool setMinuteState   = 0;
bool lastMinuteState  = 1;

#define setHourPin      3
bool setHourState     = 0;
bool lastHourState    = 1;

void setup() {
    Wire.begin();
    sevseg.begin(COMMON_CATHODE, numDigitsIn, digitPins, segmentPins, 
                 resOnSegmentsIn, updateWithDelaysIn, leadingZerosIn, disableDecPoint);
    pinMode(setMinutePin, INPUT_PULLUP);
    pinMode(setHourPin, INPUT_PULLUP);
}

void loop() {
    Wire.beginTransmission(0x68);
      Wire.write(0);
      Wire.endTransmission();
    Wire.requestFrom(0x68, 7);
      int seconds = bcdToDec(Wire.read() & 0x7f);
      int minutes = bcdToDec(Wire.read());
      int hours   = bcdToDec(Wire.read() & 0x3f); 
    if (hours == 0) hours = 12;
    if (hours > 12) hours = hours - 12;

    setHourState = digitalRead(setHourPin);
    if (setHourState != lastHourState) {
        if (setHourState == HIGH) {
            hours++;
            if (hours > 12) hours = 1;
            Wire.beginTransmission(0x68);
              Wire.write(0);
              Wire.write(decToBcd(seconds));
              Wire.write(decToBcd(minutes));
              Wire.write(decToBcd(hours));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
            Wire.endTransmission();
        } else delay(50);
    }
    lastHourState = setHourState;
    
    setMinuteState = digitalRead(setMinutePin);
    if (setMinuteState != lastMinuteState) {
        if (setMinuteState == HIGH) {
            minutes++;
            if (minutes > 59) minutes = 1;
            Wire.beginTransmission(0x68);
              Wire.write(0);
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(minutes));
              Wire.write(decToBcd(hours));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
              Wire.write(decToBcd(0));
            Wire.endTransmission();
        } else delay(50);
    }
    lastMinuteState = setMinuteState;
    
    sevseg.setNumber((hours*100)+minutes);
    sevseg.refreshDisplay();
}
