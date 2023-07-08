//
// Structure and type definitions for working with time
//
#ifndef _TIMELIB_H_
#define _TIMELIB_H_

// type to hold seconds since Jan 1, 2000
typedef uint32_t utime_t;

//structure to describe rules for when daylight/summer time begins,
//or when standard time begins.
struct TimeChangeRule
{
    uint8_t week;      //First, Second, Third, Fourth, or Last week of the month
    uint8_t dow;       //day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t month;     //1=Jan, 2=Feb, ... 12=Dec
    uint8_t hour;      //0-23
    int16_t offset;    //offset from UTC in minutes
};

// type to hold the current time, broken down into components
typedef struct  { 
  uint8_t Second; 
  uint8_t Minute; 
  uint8_t Hour; 
  uint8_t Wday;   // day of week, sunday is day 1
  uint8_t Day;
  uint8_t Month; 
  uint8_t Year;   // offset from 2000; 
} 	tmElements_t;

#endif	// _TIMELIB_H_
