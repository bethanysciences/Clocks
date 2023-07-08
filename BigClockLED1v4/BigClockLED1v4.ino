#include "RTClib.h"
RTC_DS3231 rtc;
int hours = 0;
int minutes = 0;
int seconds = 0;
byte segmentLatch = 5;  // Blue         BIG LED pin assignments
byte segmentClock = 6;  // Green     
byte segmentData = 7;   // Yellow
                        // Orange       uses 5V bus 
                        // Red          uses vin from 12v supply
                        // Black        uses gnd bus
long digit1, digit2, digit3, digit4, digit5, digit6;
long hhmmss;            

void setup() {
  Serial.begin(9600);
  rtc.begin();
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  pinMode(segmentLatch, OUTPUT);
  pinMode(segmentClock, OUTPUT);
  pinMode(segmentData, OUTPUT);
  digitalWrite(segmentLatch, LOW);
  digitalWrite(segmentClock, LOW);
  digitalWrite(segmentData, LOW);
}

void loop() {
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  hours = now.hour();
  minutes = now.minute();
  seconds = now.second();  
  if (hours == 0) hours = 12;
  if (hours > 12) hours = hours - 12;  
  digit1 = hours/10;             // 02 = 0   18 = 1
  digit2 = hours%10;             // 02 = 2   18 = 8
  digit3 = minutes/10;
  digit4 = minutes%10;
  digit5 = seconds/10;
  digit6 = seconds%10;
  hhmmss = (digit1*100000)+     // parse out to drive the digits
           (digit2*10000)+      // with leading zeros if necessary
           (digit3*1000)+
           (digit4*100)+
           (digit5*10)+
           (digit6*1);
  showTime(hhmmss);
  delay(1000);
}

void showTime(unsigned long value) {        // push digits to BIG LEDs
  unsigned long number = abs(value);
  for (byte x = 0 ; x < 6 ; x++) {
    int remainder = number % 10;
    if(x == 2 || x == 4) postNumber(remainder, true);    // decimal places
    else if(x == 5 && remainder == 0) postNumber(' ',false);
    else postNumber(remainder, false);
    number /= 10;
  }
  digitalWrite(segmentLatch, LOW);    // latch current segment
  digitalWrite(segmentLatch, HIGH);   // move storage on rising edge
}

void postNumber(byte number, boolean decimal) {
#define a  1<<0               //       ---  a 0 mapped segments to pins
#define b  1<<6               //  1 f |   | b 6
#define c  1<<5               //       ---  g 2
#define d  1<<4               //  3 e |   | c 5
#define e  1<<3               //       ---  d 4 .DP 7
#define f  1<<1
#define g  1<<2
#define dp 1<<7
  byte segments;
  switch (number) {
    case 1: segments = b | c; break;
    case 2: segments = a | b | d | e | g; break;
    case 3: segments = a | b | c | d | g; break;
    case 4: segments = f | g | b | c; break;
    case 5: segments = a | f | g | c | d; break;
    case 6: segments = a | f | g | e | c | d; break;
    case 7: segments = a | b | c; break;
    case 8: segments = a | b | c | d | e | f | g; break;
    case 9: segments = a | b | c | d | f | g; break;
    case 0: segments = a | b | c | d | e | f; break;
    case ' ': segments = 0; break;
    case 'c': segments = g | e | d; break;
    case '-': segments = g; break;
  }
  if (decimal) segments |= dp;
  for (byte x = 0 ; x < 8 ; x++) {       // clock bits out to the drivers
    digitalWrite(segmentClock, LOW);
    digitalWrite(segmentData, segments & 1 << (7 - x));
    digitalWrite(segmentClock, HIGH);    // transfer on rising edge 
  }
}
