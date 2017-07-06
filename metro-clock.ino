#include <Wire.h>
#define DS3231_I2C_ADDR     0x68    // RTC I2C address
#define OPENSEG_I2C_ADDR    0x71    // OpenSegment I2C address
#define keyPL               7
#define SetMinute           6 
#define SetHour             5

byte decToBcd(byte val) { return ( (val/10*16) + (val%10) ); }
byte bcdToDec(byte val) { return ( (val/16*10) + (val%16) ); }

void setup() {
  	Wire.begin();
      Wire.beginTransmission(OPENSEG_I2C_ADDR); 
      Wire.write('v'); 
    Wire.endTransmission();   
  	pinMode(SetMinute, INPUT_PULLUP);
  	pinMode(SetHour, INPUT_PULLUP);
  	pinMode(keyPL, INPUT_PULLUP);
}

void loop() {
  	byte second, minute, hour, day, date, month, year; 
  	getDate(&second, &minute, &hour, &day, &date, &month, &year); 

    if (hour == 0) hour = 12;
    if (hour > 12) hour = hour - 12;

    /*
    char times[40]; 
    sprintf(times, " MCU %02d:%02d:%02d", hour, minute, second);
    Serial.println(times);
    */
    
	if (!digitalRead(SetHour) && !digitalRead(keyPL)) {
      	second = 0;
      	hour++;
      	if (hour > 23) hour = 0;
      	setDate(second, minute, hour, day, date, month, year);
      	delay(200);
  	}
  	if (!digitalRead(SetMinute) && !digitalRead(keyPL)) {
      	second = 0;
      	minute++;
      	if (minute > 59) minute = 0;
      	setDate(second, minute, hour, day, date, month, year);
      	delay(200);
  	}
    
    Wire.beginTransmission(OPENSEG_I2C_ADDR);
      Wire.write(hour / 10);
      Wire.write(hour %= 10);
      Wire.write(minute / 10);
      Wire.write(minute %= 10);
    Wire.endTransmission();

    delay(500);
}

void setDate(byte second, byte minute, byte hour, byte day, 
             byte date, byte month, byte year) {
    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(0);
    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(day));
    Wire.write(decToBcd(date));
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
}

void getDate(byte *second, byte *minute, byte *hour, byte *day,
             byte *date, byte *month, byte *year) {
    Wire.beginTransmission(DS3231_I2C_ADDR);
    Wire.write(0);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDR, 7);
    *second = bcdToDec(Wire.read() & 0x7f);
    *minute = bcdToDec(Wire.read());
    *hour   = bcdToDec(Wire.read() & 0x3f); 
    *day    = bcdToDec(Wire.read());
    *date   = bcdToDec(Wire.read());
    *month  = bcdToDec(Wire.read());
    *year   = bcdToDec(Wire.read());
}

