/* SevSeg Library
 * This library allows an Arduino to easily display numbers and letters on a
 * 7-segment display without a separate 7-segment display controller.
 * Copyright 2017 Dean Reading deanreading@hotmail.com
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef MAXNUMDIGITS
#define MAXNUMDIGITS 8 // Can be increased, but the max number is 2^31
#endif

#ifndef SevSeg_h
#define SevSeg_h
#include "Arduino.h"

#define COMMON_CATHODE 0
#define COMMON_ANODE 1
#define N_TRANSISTORS 2
#define P_TRANSISTORS 3
#define NP_COMMON_CATHODE 1
#define NP_COMMON_ANODE 0

class SevSeg {
public:
  SevSeg();
  void refreshDisplay();
  void begin(byte hardwareConfig,       byte numDigitsIn,       byte digitPinsIn[],
             byte segmentPinsIn[],      bool resOnSegmentsIn=0, bool updateWithDelaysIn=0,
             bool leadingZerosIn=0,     bool disableDecPoint=0);
  void setBrightness(int brightnessIn);                             // 0 - 100
  void setNumber(long numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(unsigned long numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(int numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(unsigned int numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(char numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(byte numToShow, char decPlaces=-1, bool hex=0);
  void setNumber(float numToShow, char decPlaces=-1, bool hex=0);
  void setSegments(byte segs[]);
  void setChars(char str[]);
  void blank(void);

private:
  void setNewNum(long numToShow, char decPlaces, bool hex=0);
  void findDigits(long numToShow, char decPlaces, bool hex, byte digits[]);
  void setDigitCodes(byte nums[], char decPlaces);
  void segmentOn(byte segmentNum);
  void segmentOff(byte segmentNum);
  void digitOn(byte digitNum);
  void digitOff(byte digitNum);
  byte digitOnVal,digitOffVal,segmentOnVal,segmentOffVal;
  bool resOnSegments, updateWithDelays, leadingZeros;
  byte digitPins[MAXNUMDIGITS];
  byte segmentPins[8];
  byte numDigits;
  byte numSegments;
  byte prevUpdateIdx;
  byte digitCodes[MAXNUMDIGITS];
  unsigned long prevUpdateTime;
  int ledOnTime;
  int waitOffTime;
  bool waitOffActive;
};

#endif //SevSeg_h
/// END ///
