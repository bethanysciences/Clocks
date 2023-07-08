/*

 Demo Code for HDSP 2111 using SN74LS595N 
 Matt Joyce < matt@nycresistor.com >
 Mark Tabry 
 Scott Hamilton

 (FlorinC, May 2, 2014) adapted for HDSP-253x.
 (Mike Mitchell, March 8, 2015) adapted for WiFiChron
 
The button on the left is the Set Button, the middle button is the Up
button, the button on the right is the Down button.  To set the clock,
press the Set button to cycle through all the set options.  The first
option is alarm on/off.  The Up and Down buttons will enable and disable
the alarm.  The next option sets the alarm hours.  The two hour digits will
flash to indicate what the up/down buttons will change.  The hour is displayed
in 24-hour mode (0-23) so you can tell if you're setting for AM or PM.
Pressing the Set button again sets the minutes for the alarm.
The next set option is the time hours, followed by the time minutes, then
the time seconds.  Again the digits will flash to indicate what the Up and
Down buttons modify.  12 hour/24 hour mode is next, followed by scroll speed,
display brightness, and chime enable. With chimes enabled the clock sounds one
chirp on the half-hour and double chirp on the hour. The next set options are
Year, Month, then day-of-month. The final set option is WiFi.  It has three
possible values, 'off', 'on', and 'init'.  When set to 'on' it uses the SSID
and key that have been saved in flash to attempt to connect to an access point.
When set to 'init', there is an additional 'init y/n' option to confirm the
init.  Once in the 'init' mode the ESP8266 WiFi chip starts acting as its own
access point, using the SSID WiFiChron.  The key for WiFiChron is WifiChron
(not very secure, but better than nothing).  The clock will display an IP
address, something like "192.168.4.1".  Change your local system to use
the WiFiChron access point and web-browse to that IP address, something like
	http://192.168.4.1/
You should see a setup page in your browser.  Use that page to configure
your local access-point SSID and key.  You can also configure an optional
SNTP host for synchronizing time and an optional RSS URL.  Fill in something
like
	pool.ntp.org
for the SNTP host or 
	http://w1.weather.gov/xml/current_obs/KRDU.rss
for the RSS URL.
Clicking on the "Submit" button will send the configuration to the WiFiChron
system, which will write it into its flash.  If you check "Activate" before
submitting, the WiFiChron will immediately switch out of access-point mode and
attempt to connect to the SSID/key that you've entered.  If you have problems,
you can power-cycle the WiFiChron.  It will use whatever settings it has
stored in flash.
The battery, a CR1220 (+ side up) should last quite a bit more than
a year.

*/
 
/******************************************************************************/

/******************************************************************************/
#include <Arduino.h>
#include "digitalWriteFast.h"
#include <Wire.h>
#include "DS1307.h"
#include <avr/eeprom.h>
#include "sound.h"
#include "UserConf.h"

#ifdef WANT_ESP8266
  #include "ESP8266.h"
#endif

#ifdef WANT_GPS
  #include "GPS.h"
#endif

#ifdef WANT_PROVERBS
  #include "proverbs.h"
#endif

// used to enable/disable Serial.print, time-consuming operation;
// to minimize the code size, all references to Serial should be commented out;
//#define _DEBUG_

// display brightness definitions, indicating percentage brightness;
#define HDSP253X_BRIGHT_100         0x00
#define HDSP253X_BRIGHT_80          0x01
#define HDSP253X_BRIGHT_53          0x02
#define HDSP253X_BRIGHT_40          0x03
#define HDSP253X_BRIGHT_27          0x04
#define HDSP253X_BRIGHT_20          0x05
#define HDSP253X_BRIGHT_13          0x06
#define HDSP253X_BRIGHT_0           0x07

/*
 * Define interface pins
 */
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  /* Larger 1284P or 644P processors */

  //Pin connected to latch pin (ST_CP) of 74HC595
  #define PIN_SHIFT_LATCH	12
  //Pin connected to clock pin (SH_CP) of 74HC595
  #define PIN_SHIFT_CLOCK	1
  //Pin connected to Data in (DS) of 74HC595
  #define PIN_SHIFT_DATA	0

  //HDSP Pins
  #define PIN_HDSP_CE	5
  #define PIN_HDSP_WR	6
  #define PIN_HDSP_A0	2
  #define PIN_HDSP_A1	3
  #define PIN_HDSP_A2	4
  #define PIN_HDSP_A3	7
//  #define PIN_BTN_SET	22 // trace re-routed from 14;
  #define PIN_BTN_SET	14
  #define PIN_BTN_UP	15
  #define PIN_BTN_DOWN	18
  #define PIN_SPEAKER	13

  #define digitalWriteFast(P, V)   digitalWrite((P), (V))

#else
  /* Smaller 328P processor */

  //Pin connected to latch pin (ST_CP) of 74HC595
  #define PIN_SHIFT_LATCH	8
  //Pin connected to clock pin (SH_CP) of 74HC595
  #define PIN_SHIFT_CLOCK	12
  //Pin connected to Data in (DS) of 74HC595
  #define PIN_SHIFT_DATA	11

  //HDSP Pins
  #define PIN_HDSP_CE	5
  #define PIN_HDSP_WR	6
  #define PIN_HDSP_A0	2
  #define PIN_HDSP_A1	3
  #define PIN_HDSP_A2	4
  #define PIN_HDSP_A3	9

  #define PIN_BTN_SET	14  // A0
  #define PIN_BTN_UP	15  // A1
  #define PIN_BTN_DOWN	16  // A2
  #define PIN_SPEAKER	10  // D10
#endif

enum {
  MENU_TIME = 0,		// menu default, show time-of-day
  MENU_ALARM_ON,		// menu alarm on/off
  MENU_ALARM_HOUR,		// menu alarm hour
  MENU_ALARM_MINUTE,		// menu alarm minute
  MENU_SET_HOUR,		// menu set hour
  MENU_SET_MINUTE,		// menu set minute
  MENU_SET_SECOND,		// menu set second
  MENU_12HOUR,			// menu 12 Hour/24 Hour select
  MENU_SCROLL,			// menu scroll speed
  MENU_CHIMES,			// menu chime on/off select
#ifdef WANT_PROVERBS
  MENU_PROVERBS,		// menu enable/disable proverb display
#endif
#ifdef WANT_MOONPHASE
  MENU_MOONPHASE,		// menu enable/disable phase of moon display
#endif
  MENU_BRIGHT,			// menu display brightness
  MENU_YEAR,			// menu year
  MENU_MONTH,			// menu month
  MENU_DAY,			// menu day
  MENU_TZ_HOUR,			// timezone hour offset from GMT
  MENU_TZ_MINUTE,		// timezone minute offset from GMT
#ifdef WANT_ESP8266
  MENU_WIFI_ON,			// menu WIFI enable/disable/init
  MENU_WIFI_INIT,
#endif
  MENU_DST_ENABLE,		// Daylight savings enable
  MENU_DST_START_MONTH,		// Starting month of Daylight Savings
  MENU_DST_START_WEEK,		// Starting week of Daylight Savings
  MENU_DST_END_MONTH,		// Ending month of Daylight Savings
  MENU_DST_END_WEEK,		// Ending week of Daylight Savings
};

// storage locations for initialization parameters
#define EEPROM_BRIGHT	4
#define EEPROM_12HOUR	5
#define EEPROM_CHIMES	6
#define EEPROM_SCROLL	7
#define EEPROM_ALARM_HR	8
#define EEPROM_ALARM_MN	9
#define EEPROM_ALARM_ON	10
#define EEPROM_WIFI_ON 11
#define EEPROM_DST_ENABLE 12
#define EEPROM_DST_SM	13
#define EEPROM_DST_SW	14
#define EEPROM_DST_EM	15
#define EEPROM_DST_EW	16
#define EEPROM_TZ_HOUR	17
#define EEPROM_TZ_MINUTE 18
#define EEPROM_PROVERBS	19
#define EEPROM_MOONPHASE 20

// globals; their values get set by calling getTimeFromRTC();
int lastSecond = 0;
int alHr = 0;
int alMn = 0;
boolean alOn = false;
boolean alSounding = false;
#ifdef WANT_ESP8266
byte wifiOn = 0;
#endif

uint32_t timeLeaveMenu;
uint32_t timeNextDate;
uint32_t timeNextButton;
uint32_t timeNextScroll;
uint32_t timeNextFlash;
uint32_t timeAlarmStop;
boolean wasTimeEverSet = false;
boolean isScrolling;
boolean isFlashing;
boolean initRandom = true;
int8_t scrollPos;
byte mode = MENU_TIME;
uint8_t brite;
byte doChimes;
#ifdef WANT_PROVERBS
byte doProverbs;
#endif
#ifdef WANT_MOONPHASE
byte doMoon;
#endif
byte dispType;
byte do12Hr;
byte scrollSpeed;
byte doDST;
byte DSTsm;
byte DSTsw;
byte DSTem;
byte DSTew;
int8_t TZhr;
int8_t TZmn;
char displayBuffer[10]; //longest word + space + null
char tempBuffer[64];	//for long string to scroll (must be less than 128)
/******************************************************************************/

/*----------------------------------------------------------------------*
 * Arduino Timezone Library v1.0                                        *
 * Jack Christensen Mar 2012                                            *
 *                                                                      *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/ 
 
// API starts months from 1, this array starts from 0
const uint8_t monthDays[] PROGMEM = {31,28,31,30,31,30,31,31,30,31,30,31};

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24UL)
#define DAYS_PER_WEEK (7UL)
#define SECS_PER_WEEK (SECS_PER_DAY * DAYS_PER_WEEK)
#define SECS_PER_YEAR (SECS_PER_WEEK * 52UL)
// leap year calulator expects year argument as years offset from 2000
//#define LEAP_YEAR(Y)     ((((Y)&3)==0) && ((((Y)%100)!=0) || (((Y)%400)==0)))
#define LEAP_YEAR(Y)     (((Y)&3)==0)

#include "TimeLib.h"

struct TimeChangeRule _dst;    //rule for start of dst or summer time for any year
struct TimeChangeRule _std;    //rule for start of standard time for any year
utime_t _dstUTC;         //dst start for given/current year, given in UTC
utime_t _stdUTC;         //std time start for given/current year, given in UTC
utime_t _dstLoc;         //dst start for given/current year, given in local time
utime_t _stdLoc;         //std time start for given/current year, given in local time
uint8_t _dstUTCy;	 //year of dst start

/*----------------------------------------------------------------------*
 * Create a Timezone object from the given time change rules.           *
 *----------------------------------------------------------------------*/
void Timezone(struct TimeChangeRule *dstStart, struct TimeChangeRule *stdStart)
{
    _dst = *dstStart;
    _std = *stdStart;
}

// break the given time_t into time components
// this is a more compact version of the C library localtime function
// note that year is offset from 2000 !!!
void breakTime(utime_t time, tmElements_t *tm)
{
  uint8_t year;
  uint8_t month, monthLength;
  uint32_t days;
  
  tm->Second = time % 60;
  time /= 60; // now it is minutes
  tm->Minute = time % 60;
  time /= 60; // now it is hours
  tm->Hour = time % 24;
  time /= 24; // now it is days
  tm->Wday = ((time + 6) % 7) + 1;  // Sunday is day 1 
  
  year = 0;  
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= (unsigned)time) {
    year++;
  }
  tm->Year = year; // year is offset from 2000 
  
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0

  days=0;
  month=0;
  monthLength=0;
  for (month=0; month<12; month++) {
    // February is month 1 in table
    if ((month == 1) && LEAP_YEAR(year)) {
        monthLength=29;
    } else {
      monthLength = pgm_read_byte(&monthDays[month]);
    }
    
    if (time >= monthLength) {
      time -= monthLength;
    } else {
        break;
    }
  }
  tm->Month = month + 1;  // jan is month 1  
  tm->Day = time + 1;     // day of month
}

uint8_t time_t_year(utime_t t)
{
  uint8_t year;
  uint32_t days;

  t /= SECS_PER_DAY;
  year = 0;  
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= (unsigned)t) {
    year++;
  }
  return year;
}

// assemble time elements into time_t 
// note year argument is offset from 2000
utime_t makeTime(tmElements_t *tm)
{   
  uint8_t i;
  utime_t seconds;

  // seconds from 2000 till 1 jan 00:00:00 of the given year
  seconds = tm->Year*(SECS_PER_DAY * 365);
  for (i = 0; i < tm->Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }
  
  // add days for this year, months start from 1
  for (i = 1; i < tm->Month; i++) {
    if ( (i == 2) && LEAP_YEAR(tm->Year)) { 
      seconds += SECS_PER_DAY * 29;
    } else {
      //monthDay array starts from 0
      seconds += SECS_PER_DAY * pgm_read_byte(&monthDays[i-1]);
    }
  }
  seconds += (tm->Day-1) * SECS_PER_DAY;
  seconds += tm->Hour * SECS_PER_HOUR;
  seconds += tm->Minute * SECS_PER_MIN;
  seconds += tm->Second;
  return seconds; 
}

/*----------------------------------------------------------------------*
 * Convert the given DST change rule to a time_t value                  *
 * for the given year.                                                  *
 *----------------------------------------------------------------------*/
utime_t toTime_t(struct TimeChangeRule *r, uint8_t yr)
{
    tmElements_t tm;
    utime_t t;
    uint8_t m, w;            //temp copies of r.month and r.week

    m = r->month;
    w = r->week;
    if (w == 0) {            //Last week = 0
        if (++m > 12) {      //for "Last", go to the next month
            m = 1;
            yr++;
        }
        w = 1;               //and treat as first week of next month, subtract 7 days later
    }

    tm.Hour = r->hour;
    tm.Minute = 0;
    tm.Second = 0;
    tm.Day = 1;
    tm.Month = m;
    tm.Year = yr;
    t = makeTime(&tm);        //first day of the month, or first day of next month for "Last" rules
    tm.Wday = ((t/SECS_PER_DAY + 6) % 7) + 1;  // Sunday is day 1 
    t += (7 * (w - 1) + (r->dow - tm.Wday + 7) % 7) * SECS_PER_DAY;
    if (r->week == 0) t -= 7 * SECS_PER_DAY;    //back up a week if this is a "Last" rule
    return t;
}

/*----------------------------------------------------------------------*
 * Calculate the DST and standard time change points for the given      *
 * given year as local and UTC time_t values.                           *
 *----------------------------------------------------------------------*/
void calcTimeChanges(uint8_t yr)
{
    _dstLoc = toTime_t(&_dst, yr);
    _stdLoc = toTime_t(&_std, yr);
    _dstUTC = _dstLoc - (_std.offset * SECS_PER_MIN);
    _stdUTC = _stdLoc - (_dst.offset * SECS_PER_MIN);
}

/*------------------------------------------------------*
 * Determine whether the given UTC time_t is within the DST interval    *
 * or the Standard time interval.                                       *
 *----------------------------------------------------------------------*/
boolean utcIsDST(utime_t utc)
{
    uint8_t yr = time_t_year(utc);

    //recalculate the time change points if needed
    if (_dstUTCy != yr) calcTimeChanges(yr);

    if (_stdUTC > _dstUTC)    //northern hemisphere
        return (utc >= _dstUTC && utc < _stdUTC);
    else                      //southern hemisphere
        return !(utc >= _stdUTC && utc < _dstUTC);
}

/*----------------------------------------------------------------------*
 * Convert the given UTC time to local time, standard or                *
 * daylight time, as appropriate.                                       *
 *----------------------------------------------------------------------*/
utime_t toLocal(utime_t utc)
{
    uint8_t yr = time_t_year(utc);

    //recalculate the time change points if needed
    if (_dstUTCy != yr) calcTimeChanges(yr);

    if (utcIsDST(utc))
        return utc + _dst.offset * SECS_PER_MIN;
    else
        return utc + _std.offset * SECS_PER_MIN;
}

/*
 ** End of timezone library routines
 */

utime_t locTt, utcTt;
tmElements_t locTm, utcTm;

/******************************************************************************/
// set the appropriate TimeZone rule for this locale and DST_enable settings
void setTzRule()
{
    struct TimeChangeRule rls, rle;

    /* When DST ends */
    rle.week = DSTew;	/* First week */
    rle.dow = 1;	/* Sunday */
    rle.month = DSTem;	/* November */
    rle.hour = 2;	/* 2:00 AM */
    rle.offset = (TZhr * 60) + TZmn;

    /* When DST starts */
    rls.week = DSTsw;	/* Second week */
    rls.dow = 1;	/* Sunday */
    rls.month = DSTsm;	/* March */
    rls.hour = 2;	/* 2:00 AM */
    rls.offset = rle.offset + 60;

    _dstUTCy = 0;	/* make sure calcTimeChanges is called */

    if (doDST != 0) {
	Timezone(&rls, &rle);
    } else {
	Timezone(&rle, &rle);
    }
}
/******************************************************************************/

/******************************************************************************/
// Read the entire RTC buffer
void getTimeFromRTC(int ls)
{
  int rtc[7];
  utime_t tt;

  RTC_DS1307.get(rtc, true);

  if (ls != rtc[DS1307_SEC]) {
    // check to avoid glitches;
    if (rtc[DS1307_MIN] < 60 && rtc[DS1307_HR] < 24 && rtc[DS1307_SEC] < 60) {
      utcTm.Second = rtc[DS1307_SEC];
      utcTm.Minute = rtc[DS1307_MIN];
      utcTm.Hour   = rtc[DS1307_HR];
    }

    // check to avoid glitches;
    if (rtc[DS1307_YR] <= 2099 && rtc[DS1307_MTH] <= 12  && rtc[DS1307_DATE] <= 31) {
      utcTm.Wday   = rtc[DS1307_DOW];
      utcTm.Day    = rtc[DS1307_DATE];
      utcTm.Month  = rtc[DS1307_MTH];
      utcTm.Year   = rtc[DS1307_YR] - 2000;
    }
  
    // convert UTC to local time
    utcTt = makeTime(&utcTm);
    locTt = toLocal(utcTt);
    breakTime(locTt, &locTm);
  }
  
  // The RTC may have a dead battery or may have never been initialized
  // If so, the RTC doesn't run until it is set.
  // Here we check once to see if it is running and start it if not.
  if (!wasTimeEverSet) {
    wasTimeEverSet = true;
    if (locTm.Hour == 0 && locTm.Minute == 0 && locTm.Second == 0) {
      // set an arbitrary time to get the RTC going;
      utcTm.Year = 16;
      utcTm.Month = 1;
      utcTm.Day = 1;
      utcTm.Wday = zellersDow(&utcTm);
      utcTm.Hour = 12;
      utcTm.Minute = 34;
      utcTm.Second = 56;
      setTime(&utcTm);
      setDate(&utcTm);
    }
  }  
#ifdef _DEBUG_
  Serial.print("Time is ");
  Serial.print(rtc[DS1307_HR]);
  Serial.print(":");
  Serial.print(rtc[DS1307_MIN]);
  Serial.print(":");
  Serial.println(rtc[DS1307_SEC]);

  Serial.print("Date is ");
  Serial.print(rtc[DS1307_DOW]);
  Serial.print(" ");
  Serial.print(rtc[DS1307_YR]);
  Serial.print("/");
  Serial.print(rtc[DS1307_MTH]);
  Serial.print("/");
  Serial.println(rtc[DS1307_DATE]);
#endif
}
/******************************************************************************/

/******************************************************************************/
void setTime(tmElements_t *tm)
{  
  // NOTE: when setting, year is 2 digits; when reading, year is 4 digits;
  RTC_DS1307.stop();
  RTC_DS1307.set(DS1307_SEC,  tm->Second);
  RTC_DS1307.set(DS1307_MIN,  tm->Minute);
  RTC_DS1307.set(DS1307_HR,   tm->Hour);
  RTC_DS1307.start();
}
/******************************************************************************/

/******************************************************************************/
void setDate(tmElements_t *tm)
{
  RTC_DS1307.stop();
  RTC_DS1307.set(DS1307_YR,   tm->Year);
  RTC_DS1307.set(DS1307_MTH,  tm->Month);
  RTC_DS1307.set(DS1307_DATE, tm->Day);
  RTC_DS1307.set(DS1307_DOW,  tm->Wday);
  RTC_DS1307.start();
}
/******************************************************************************/

/******************************************************************************/
int8_t zellersDow(tmElements_t *tm)
{
  // return the day of week based on the current date
  int16_t m, y, wd;
  y = tm->Year;
  if (y > 100) y -= 2000;
  m = tm->Month;
  if (m < 3) {	// move Jan, Feb to end of previous year
    y--;
    m += 12;
  }
  wd = tm->Day;
  wd += 13 * (m + 1) / 5;
  wd += (y * 5) / 4;
  wd %= 7;
  // dow is now 0 for Sat, 6 for Fri.  Convert to 1 for Sun, 7 for Sat
  if (wd == 0) wd = 7;
  return(wd);
}
/******************************************************************************/


/******************************************************************************/
#ifdef WANT_BLUETOOTH
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  // set the serial interface to Serial1
  #define BT_Port Serial1
#else
  // set the serial interface to Serial (only one on 328P)
  #define BT_Port Serial
#endif
char    bt_buf[80];	/* Blue Tooth message buffer */
int8_t  bt_ipos;	/* input position in buffer */
int8_t  bt_opos;	/* output position in buffer */
boolean bt_valid;	/* message should be output */
boolean bt_cmd;		/* reach end-of-line on input */

//
// Initialize bluetooth
void BTinit()
{
  BT_Port.begin(9600);
  bt_ipos = 0;
  bt_valid = false;
  bt_cmd = false;
}

// Read any pending bytes from bluetooth
//
void checkBlueTooth()
{
  char c;

  while(BT_Port.available() > 0) {
    c = BT_Port.read();

    if ((c == '\r') || (c == '\n')) {			// if CR or LF (end-of-line)
      bt_cmd = true;
      bt_ipos = 0;					// get ready for another message
    } else if (bt_ipos < sizeof(bt_buf)-1) {
      bt_buf[bt_ipos++] = c;
      bt_buf[bt_ipos] = 0;
      bt_valid = false;
    }
  }

  if (bt_cmd) {
    bt_cmd = false;
    if (strcasecmp_P(bt_buf, PSTR("CLS")) == 0) {
      memset(bt_buf, 0, sizeof(bt_buf));
    } else if ((strncasecmp_P(bt_buf, PSTR("SET TIME = "), 11) == 0)
	   &&  (bt_buf[13] == ':')
	   &&  (bt_buf[16] == ':')
    ) {
      int8_t hr = atoi(&bt_buf[11]);
      int8_t mn = atoi(&bt_buf[14]);
      int8_t sc = atoi(&bt_buf[17]);
      if ((hr >= 0) && (hr <= 23)
      &&  (mn >= 0) && (mn <= 59)
      &&  (sc >= 0) && (sc <= 59)) {
	if ((hr != locTm.Hour)
	||  (mn != locTm.Minute)
	||  (sc != locTm.Second)
	) {
	  utime_t delta;
	  delta = locTt - utcTt;

	  locTm.Hour = hr;
	  locTm.Minute = mn;
	  locTm.Second = sc;
	  locTt = makeTime(&locTm);
	  utcTt = locTt - delta;
	  breakTime(utcTt, &utcTm);
	  setTime(&utcTm);
	}
      }
    }
    else if ((strncasecmp_P(bt_buf, PSTR("SET ALARM = "), 12) == 0)
         &&  (bt_buf[14] == ':')
    ) {
      int8_t hr = atoi(&bt_buf[12]);
      int8_t mn = atoi(&bt_buf[15]);
      if ((hr >= 0) && (hr <= 23) && (mn >= 0) && (mn <= 59)) {
	if (hr != alHr) {
	  alHr = hr;
	  eeprom_write_byte((uint8_t *)EEPROM_ALARM_HR, alHr);
	}
	if (mn != alMn) {
	  alMn = mn;
	  eeprom_write_byte((uint8_t *)EEPROM_ALARM_MN, alMn);
	}
      }
    }
    else if (strcasecmp_P(bt_buf, PSTR("ENABLE ALARM")) == 0) {
      if (!alOn) {
	alOn = true;
	eeprom_write_byte((uint8_t *)EEPROM_ALARM_ON, alOn);
      }
    }
    else if (strcasecmp_P(bt_buf, PSTR("DISABLE ALARM")) == 0) {
      if (alOn) {
	alOn = false;
	eeprom_write_byte((uint8_t *)EEPROM_ALARM_ON, alOn);
      }
      if (alSounding) {
	silenceAlarm();
	alSounding = false;
      }
    }
    else if (strncasecmp_P(bt_buf, PSTR("SILENCE ALARM"), 13) == 0) {
      if (alSounding) {
	silenceAlarm();
	alSounding = false;
      }
    }
    else {		// message not a command, so mark as valid for display
      bt_valid = true;
    }
  }
}

/*
 * Put the current bluetooth message in the display buffer
 */
void BTgetMsg(char *buf, int8_t sz)
{
  int8_t i;
  char c;

  bt_opos = 0;
  i = 0;

  do {
    c = bt_buf[bt_opos++];
    buf[i++] = c;
  } while((c != 0) && (i < sz) && (bt_opos < sizeof(bt_buf)));
}

/*
 * Put the next few bytes of the bluetooth message into the display buffer
 */
void BTnextMsgPart(char *buf, int8_t sz)
{
  int8_t i;
  char c;

  i = 0;
  if (bt_valid) {
    do {
      c = bt_buf[bt_opos++];
      buf[i++] = c;
    } while((c != 0) && (i < sz) && (bt_opos < sizeof(bt_buf)));
  }
}
/******************************************************************************/
#endif	/* WANT_BLUETOOTH */

/******************************************************************************/
// set the HDSP display brightness
void setBrightness()
{
  if (brite >= HDSP253X_BRIGHT_0) {
    brite = HDSP253X_BRIGHT_13; // don't let display turn off completely
  }
  digitalWrite(PIN_HDSP_A3, LOW);
  digitalWrite(PIN_SHIFT_LATCH, LOW);
  shiftOut(PIN_SHIFT_DATA, PIN_SHIFT_CLOCK, MSBFIRST, brite);
  digitalWrite(PIN_SHIFT_LATCH, HIGH);
  delay(20);    
  digitalWrite(PIN_HDSP_CE, LOW);
  delay(20);
  digitalWrite(PIN_HDSP_WR, LOW);
  delay(20); 
  digitalWrite(PIN_HDSP_WR, HIGH);
  delay(20);
  digitalWrite(PIN_HDSP_CE, HIGH);
  delay(20);
  digitalWrite(PIN_HDSP_A3, HIGH);
}
/******************************************************************************/


/******************************************************************************/
void setup()
{
  Serial.begin(9600);
#ifdef _DEBUG_
  Serial.println("in setup");
#endif

#ifdef __AVR_ATmega1284P__
  uint8_t tmp = 1<<JTD;		// Disable JTAG
  MCUCR = tmp;			// Disable JTAG
  MCUCR = tmp;			// Disable JTAG
#endif

  //set pins to output because they are addressed in the main loop
  pinMode(PIN_SHIFT_LATCH, OUTPUT);
  pinMode(PIN_SHIFT_DATA, OUTPUT);  
  pinMode(PIN_SHIFT_CLOCK, OUTPUT);
  pinMode(PIN_HDSP_A0, OUTPUT);
  pinMode(PIN_HDSP_A1, OUTPUT);
  pinMode(PIN_HDSP_A2, OUTPUT);
  pinMode(PIN_HDSP_A3, OUTPUT);
  pinMode(PIN_HDSP_CE, OUTPUT);
  pinMode(PIN_HDSP_WR, OUTPUT);
  digitalWrite(PIN_HDSP_CE, HIGH);
  digitalWrite(PIN_HDSP_WR, HIGH);
  
  // buttons to set hours and minutes are on A1 (D15) and A2(D16) respectively;
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  /* Larger 1284P or 644P processors */
  // all inputs have 10k external pull-up resistor
  pinMode(PIN_BTN_SET,  INPUT);
  pinMode(PIN_BTN_UP,   INPUT);
  pinMode(PIN_BTN_DOWN, INPUT);
#else
  /* Smaller 328P processors */
  pinMode(PIN_BTN_SET,  INPUT_PULLUP);
  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
#endif

  setupSpeaker(PIN_SPEAKER);

  // read EEPROM data to initialize settings

  brite = eeprom_read_byte((uint8_t *)EEPROM_BRIGHT);
  setBrightness();

  timeNextDate = millis() + 5000;

  do12Hr = eeprom_read_byte((uint8_t *)EEPROM_12HOUR);
  if (do12Hr > 1) {
    do12Hr = 0; // start in 24 hour display mode, not 12 hour
  }

  scrollSpeed = eeprom_read_byte((uint8_t *)EEPROM_SCROLL);
  if (scrollSpeed > 9) {
    scrollSpeed = 5;
  }

  doChimes = eeprom_read_byte((uint8_t *)EEPROM_CHIMES);

#ifdef WANT_PROVERBS
  doProverbs = eeprom_read_byte((uint8_t *)EEPROM_PROVERBS);
#endif

#ifdef WANT_MOONPHASE
  doMoon = eeprom_read_byte((uint8_t *)EEPROM_MOONPHASE);
#endif

  alOn = eeprom_read_byte((uint8_t *)EEPROM_ALARM_ON);
  alHr = eeprom_read_byte((uint8_t *)EEPROM_ALARM_HR);
  alMn = eeprom_read_byte((uint8_t *)EEPROM_ALARM_MN);
  if (alHr > 23 || alHr < 0) alHr = 0;
  if (alMn > 59 || alMn < 0) alMn = 0;

#ifdef WANT_ESP8266
  wifiOn = eeprom_read_byte((uint8_t *)EEPROM_WIFI_ON);
  if (wifiOn > 1 || wifiOn < 0) wifiOn = 0;
#endif

  /* DST enabled in this locale? */
  doDST = eeprom_read_byte((uint8_t *)EEPROM_DST_ENABLE);
  if (doDST > 1) doDST = 1;
  DSTsm = eeprom_read_byte((uint8_t *)EEPROM_DST_SM);
  if (DSTsm < 1 || DSTsm > 12) DSTsm = 3;	/* start in March by default */
  DSTsw = eeprom_read_byte((uint8_t *)EEPROM_DST_SW);
  if (DSTsw > 4) DSTsw = 2;			/* Start in Week 2 by default */
  DSTem = eeprom_read_byte((uint8_t *)EEPROM_DST_EM);
  if (DSTem < 1 || DSTem > 12) DSTem = 11;	/* End in November by default */
  DSTew = eeprom_read_byte((uint8_t *)EEPROM_DST_EW);
  if (DSTew > 4) DSTew = 1;			/* End in Week 1 by default */
  TZhr = (int8_t)eeprom_read_byte((uint8_t *)EEPROM_TZ_HOUR);
  if (TZhr < -12 || TZhr > 14) TZhr = 0;
  TZmn = (int8_t)eeprom_read_byte((uint8_t *)EEPROM_TZ_MINUTE);
  if (TZmn < -3 || TZmn > 3) TZmn = 0;
  TZmn *= 15;
  
  dispType = 7;	// start with date display
  mode = MENU_TIME;	// start clock in "time" mode inside "LOOP" routine
  timeNextButton = millis() + 100;
  isScrolling = false;
  isFlashing = false;
#ifdef WANT_ESP8266
  esp8266.init();
  if (wifiOn == 1) esp8266.wifiOn();
#endif
#ifdef WANT_GPS
  gps.init();
#endif
#ifdef WANT_BLUETOOTH
  BTinit();
#endif
  setTzRule();

  soundChimeShort();
}
/******************************************************************************/

/******************************************************************************/
// send the 8 characters in the displayBuffer to the display
void writeDisplay()
{
  for (int8_t i=0; i<8; i++) {    
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  /* Larger 1284P or 644P processors */
    digitalWrite(PIN_HDSP_A0, (1&i)!=0?HIGH:LOW);
    digitalWrite(PIN_HDSP_A1, (2&i)!=0?HIGH:LOW);
    digitalWrite(PIN_HDSP_A2, (4&i)!=0?HIGH:LOW);
    digitalWrite(PIN_HDSP_CE, LOW);
    // write data to the bus;
    digitalWrite(PIN_SHIFT_LATCH, LOW);
    shiftOut(PIN_SHIFT_DATA, PIN_SHIFT_CLOCK, MSBFIRST, displayBuffer[i] );
    digitalWrite(PIN_SHIFT_LATCH, HIGH);
    digitalWrite(PIN_HDSP_WR, LOW);
    digitalWrite(PIN_HDSP_WR, HIGH);
    digitalWrite(PIN_HDSP_CE, HIGH);
#else
  /* Smaller 328P processors */
    digitalWriteFast(PIN_HDSP_A0, (1&i)!=0?HIGH:LOW);
    digitalWriteFast(PIN_HDSP_A1, (2&i)!=0?HIGH:LOW);
    digitalWriteFast(PIN_HDSP_A2, (4&i)!=0?HIGH:LOW);
    digitalWriteFast(PIN_HDSP_CE, LOW);
    // write data to the bus;
    digitalWriteFast(PIN_SHIFT_LATCH, LOW);
    shiftOut(PIN_SHIFT_DATA, PIN_SHIFT_CLOCK, MSBFIRST, displayBuffer[i] );
    digitalWriteFast(PIN_SHIFT_LATCH, HIGH);
    digitalWriteFast(PIN_HDSP_WR, LOW);
    digitalWriteFast(PIN_HDSP_WR, HIGH);
    digitalWriteFast(PIN_HDSP_CE, HIGH);
#endif
  }
}
/******************************************************************************/

/******************************************************************************/
/*
 * scroll a message across the display
 * if flg is true, message comes from proverb list
 * otherwise, message comes from memory
 */
void scrollDisplay()
{
  isScrolling = true;
  scrollPos = 0;
  timeNextScroll = millis() + 256;
}

void doScroll(uint32_t ms)
{
  char c, s;
  boolean end;
  int8_t j;

  if (!isScrolling) return;
  
  end = false;
  s = tempBuffer[scrollPos];

  for (j = 0; j<8; j++) {
    if (!end) {
      c = tempBuffer[scrollPos+j];
      if (c == 0) end = true;
    }
    displayBuffer[j] = end ? ' ' : c;
  }
  writeDisplay();

  if (s == 0) {
    isScrolling = false;
    timeNextDate = ms + 15000;
    return;
  }

  scrollPos++;
  j = sizeof(tempBuffer) / 2;
  if (scrollPos >= j) {
    memmove(tempBuffer, &tempBuffer[j], j);	// move half the buffer down
    if (dispType == 1) {
#ifdef WANT_PROVERBS
      nextProverbPart(&tempBuffer[j], j);
#endif
    } else if (dispType == 2) {
#ifdef WANT_ESP8266
      esp8266.nextMsgPart(&tempBuffer[j], j);
#endif
#ifdef WANT_BLUETOOTH
      BTnextMsgPart(&tempBuffer[j], j);
#endif
    }
    scrollPos = 0;
  }
  timeNextScroll = ms + 85 + (scrollSpeed*23);
}
/******************************************************************************/

/******************************************************************************/
void getCurrentDate(tmElements_t *tm, char *bp)
{
  char c;
  int8_t i;
  
  loadDOW(tm);				// get current day of week name
  for(i=0;(c=displayBuffer[i]) != 0; i++)
    if (c != ' ') *bp++ = c;
  *bp++ = ' ';				// add space separator

  loadMonth(tm, bp);			// get current month name
  while(*bp != 0) bp++;
  
  sprintf_P(bp, PSTR(" %d"), tm->Day);  // current day of month
  while(*bp != 0) bp++;

  sprintf_P(bp, PSTR(", %d"), tm->Year + 2000);
  while(*bp != 0) bp++;
  
#ifdef WANT_ESP8266
  if (esp8266.sntpSynced) {	// add an up-arrow if time is synced with SNTP
    *bp++ = ' ';
    *bp++ = 0x5e;	// caret -- shows as up-arrow
    *bp = 0;
  }
#endif
#ifdef WANT_GPS
  if (gps.synced) {		// add an up-arrow if time is synced with GPS
    *bp++ = ' ';
    *bp++ = 0x5e;	// caret -- shows as up-arrow
    *bp = 0;
  }
#endif
}
/******************************************************************************/

/******************************************************************************/
const char SunNM[] PROGMEM = "Sunday  ";
const char MonNM[] PROGMEM = "Monday  ";
const char TueNM[] PROGMEM = "Tuesday ";
const char WedNM[] PROGMEM = "Wednesday";
const char ThuNM[] PROGMEM = "Thursday";
const char FriNM[] PROGMEM = "Friday  ";
const char SatNM[] PROGMEM = "Saturday";

const char * const dayNames[] PROGMEM = {
  SunNM, MonNM, TueNM, WedNM, ThuNM, FriNM, SatNM
};

// load day of week into buffer
void loadDOW(tmElements_t *tm)
{
  if ((tm->Wday > 0) && (tm->Wday <= 7))
    strcpy_P(displayBuffer, (char *)pgm_read_word(&dayNames[tm->Wday-1]));
  else {
    memset(displayBuffer,' ', 8);  
    displayBuffer[9] = 0;
  }
}

/******************************************************************************/

/******************************************************************************/
const char JanNM[] PROGMEM = "January";
const char FebNM[] PROGMEM = "February";
const char MarNM[] PROGMEM = "March";
const char AprNM[] PROGMEM = "April";
const char MayNM[] PROGMEM = "May";
const char JunNM[] PROGMEM = "June";
const char JulNM[] PROGMEM = "July";
const char AugNM[] PROGMEM = "August";
const char SepNM[] PROGMEM = "September";
const char OctNM[] PROGMEM = "October";
const char NovNM[] PROGMEM = "November";
const char DecNM[] PROGMEM = "December";

const char * const monthNames[] PROGMEM = {
  JanNM, FebNM, MarNM, AprNM, MayNM, JunNM,
  JulNM, AugNM, SepNM, OctNM, NovNM, DecNM,
};

// load month into buffer
void loadMonth(tmElements_t *tm, char *cp)
{
  if ((tm->Month > 0) && (tm->Month <= 12)) {
    strcpy_P(cp, (char *)pgm_read_word(&monthNames[tm->Month-1]));
  } else {
    memset(cp,' ', 8);  
    cp[9] = 0;
  }
}
/******************************************************************************/

#ifdef WANT_MOONPHASE
/*
 * Display the moon phase
 */
const char Moon1[] PROGMEM = "Full";
const char Moon2[] PROGMEM = "Waning Gibbous";
const char Moon3[] PROGMEM = "Third Quarter";
const char Moon4[] PROGMEM = "Waning Crescent";
const char Moon5[] PROGMEM = "New";
const char Moon6[] PROGMEM = "Waxing Crescent";
const char Moon7[] PROGMEM = "First Quarter";
const char Moon8[] PROGMEM = "Waxing Gibbous";

const char *moonNames[] = {
    Moon1, Moon2, Moon3, Moon4, Moon5, Moon6, Moon7, Moon8
};

/*
 * Moon phase cycles aproximately every 29.53 days.
 * It varies depending on the relative position of the sun, earth, and moon,
 * by +/- 24 hours. Over the long term it is highly accurate, but not so much
 * over the short term. The accurate way to calculate involves squares, cubes,
 * and trig functions. I took a table of Full moon times for 2000 through 2100
 * and found that If I use 2551466 seconds per cycle I'm never off by
 * more than 17.5 hours with an average error of 99 seconds.

 * current phase = (((utcTt - offset) % 2551466) * 8) / 2551466;
 * Moon was full at 21 Jan 2000, 4:42 AM
 * Offset from Full is (20 * 86400) + (4*3600) + (42*60) = 1744920
 *
 * We want to report on the moon phase just before it happens and just
 * after, so we offset by 1/2 of a phase, 1/16 of a cycle (159467 seconds)
 *
 */
#define LUNAR_CYCLE     2551466

void getCurrentMoon(uint32_t tt, char *bp)
{
  uint32_t phase, percent;
  boolean dir;

  phase = (tt - 1744920);	/* seconds since full moon */
  /* calculate the percent of full */
  percent = phase;
  percent %= LUNAR_CYCLE;
  percent = ((200 * percent) + (LUNAR_CYCLE/2)) / LUNAR_CYCLE;

  if (percent <= 100) {
    /* decreasing from Full */
    percent = 100 - percent;
    dir = false;
  } else {
    /* increasing to Full */
    percent = percent - 100;
    dir = true;
  }
  /*
   * calculate the moon phase
   */
  phase += (LUNAR_CYCLE/16);	/* offset by half a phase */
  phase %= LUNAR_CYCLE;		/* seconds within phase */
  phase *= 8;			/* eight phases */
  phase /= LUNAR_CYCLE;		/* number 0 - 7 */

  strcpy_P(bp, moonNames[phase]);
  sprintf_P(&bp[strlen(bp)], PSTR(" Moon   %ld%% of Full"), percent);
  if ((percent != 0) && (percent != 100)) {
    if (dir) {
      strcpy_P(&bp[strlen(bp)], PSTR(" and increasing"));
    } else {
      strcpy_P(&bp[strlen(bp)], PSTR(" and decreasing"));
    }
  }
}


#endif

/******************************************************************************/

/******************************************************************************/
// format time display
void dispTime(tmElements_t *tm)
{
    char colon = '.';
    int mhour = tm->Hour;
    if ((mode == MENU_TIME) && do12Hr) {
      colon = ':';
      if (mhour > 12) {
	mhour -= 12;
      }
      if (mhour == 0) mhour = 12;
    }
    sprintf_P(displayBuffer, PSTR("%02d%c%02d%c%02d"),
	      mhour, colon, tm->Minute, colon, tm->Second);
}
/******************************************************************************/

/******************************************************************************/
// update display for the current mode
void dispMode(uint32_t ms)
{
  int b = 0;
  boolean oldFlash;

  oldFlash = isFlashing;
  isFlashing = false;
  switch(mode) {
    case MENU_ALARM_ON:
      sprintf_P(displayBuffer, PSTR("Alrm %s"), alOn ? "on ": "off");
      break;

    case MENU_ALARM_HOUR:
      isFlashing = true;
      sprintf_P(displayBuffer, PSTR("Al %02d.%02d"), alHr, alMn);
      if ((ms & 128) == 0) {
	displayBuffer[3] = ' ';
	displayBuffer[4] = ' ';
      }
      break;

    case MENU_ALARM_MINUTE:
      isFlashing = true;
      sprintf_P(displayBuffer, PSTR("Al %02d.%02d"), alHr, alMn);
      if ((ms & 128) == 0) {
	displayBuffer[6] = ' ';
	displayBuffer[7] = ' ';
      }
      break;

    case MENU_SET_HOUR:
      isFlashing = true;
      dispTime(&locTm);
      if ((ms & 128) == 0) {
	displayBuffer[0] = ' ';
	displayBuffer[1] = ' ';
      }
      break;

    case MENU_SET_MINUTE:
      isFlashing = true;
      dispTime(&locTm);
      if ((ms & 128) == 0) {
	displayBuffer[3] = ' ';
	displayBuffer[4] = ' ';
      }
      break;

    case MENU_SET_SECOND:
      isFlashing = true;
      dispTime(&locTm);
      if ((ms & 128) == 0) {
	displayBuffer[6] = ' ';
	displayBuffer[7] = ' ';
      }
      break;

    case MENU_12HOUR:
      sprintf_P(displayBuffer, PSTR("%d Hour "), do12Hr ? 12 : 24);
      break;

    case MENU_SCROLL:
      sprintf_P(displayBuffer, PSTR("Scroll %d"), 9 - scrollSpeed);
      break;

    case MENU_BRIGHT:
      if (brite >= HDSP253X_BRIGHT_0)
	brite = HDSP253X_BRIGHT_13;
      b = 0;
      if      (brite == HDSP253X_BRIGHT_100) b = 99;
      else if (brite == HDSP253X_BRIGHT_80)  b = 80;
      else if (brite == HDSP253X_BRIGHT_53)  b = 53;
      else if (brite == HDSP253X_BRIGHT_40)  b = 40;
      else if (brite == HDSP253X_BRIGHT_27)  b = 27;
      else if (brite == HDSP253X_BRIGHT_20)  b = 20;
      else if (brite == HDSP253X_BRIGHT_13)  b = 13;
      sprintf_P(displayBuffer, PSTR("Brite %02d"), b);
      break;

    case MENU_CHIMES:
      sprintf_P(displayBuffer, PSTR("Chimes %c"), doChimes ? 'Y' : 'N');
      break;

#ifdef WANT_PROVERBS
    case MENU_PROVERBS:
      sprintf_P(displayBuffer, PSTR("Provrb %c"), doProverbs ? 'Y' : 'N');
      break;
#endif

#ifdef WANT_MOONPHASE
    case MENU_MOONPHASE:
      sprintf_P(displayBuffer, PSTR("Moon   %c"), doMoon ? 'Y' : 'N');
      break;
#endif

    case MENU_YEAR:
      sprintf_P(displayBuffer, PSTR("Yr  %4d"), locTm.Year + 2000);
      break;

    case MENU_MONTH:
      sprintf_P(displayBuffer, PSTR("Month %2d"), locTm.Month);
      break;

    case MENU_DAY:
      sprintf_P(displayBuffer, PSTR("Date  %2d"), locTm.Day);
      break;

    case MENU_TZ_HOUR:
      sprintf_P(displayBuffer, PSTR("TzHr %+3d"), TZhr);
      break;

    case MENU_TZ_MINUTE:
      sprintf_P(displayBuffer, PSTR("TzMn %+3d"), TZmn);
      break;

    case MENU_DST_ENABLE:
      sprintf_P(displayBuffer, PSTR("DST En %c"), doDST ? 'Y' : 'N');
      break;

    case MENU_DST_START_MONTH:
      sprintf_P(displayBuffer, PSTR("DSTsm %2d"), DSTsm);
      break;

    case MENU_DST_START_WEEK:
      sprintf_P(displayBuffer, PSTR("DSTsw %2d"), DSTsw);
      break;

    case MENU_DST_END_MONTH:
      sprintf_P(displayBuffer, PSTR("DSTem %2d"), DSTem);
      break;

    case MENU_DST_END_WEEK:
      sprintf_P(displayBuffer, PSTR("DSTew %2d"), DSTew);
      break;

#ifdef WANT_ESP8266
    case MENU_WIFI_ON:
      if (wifiOn == 0) {
	strcpy_P(displayBuffer, PSTR("WiFi Off"));
      } else if (wifiOn == 1) {
	strcpy_P(displayBuffer, PSTR("WiFi On "));
      } else {
	strcpy_P(displayBuffer, PSTR("WiFi Ini"));
      }
      break;

    case MENU_WIFI_INIT:
      sprintf_P(displayBuffer, PSTR("Init?  %c"), wifiOn == 2 ? 'N' : 'Y');
      break;
#endif

    default:
      dispTime(&locTm);
      mode = MENU_TIME;
      break;
  }
  writeDisplay();
  if (isFlashing != oldFlash)
    timeNextFlash = ms + 128;
}
/******************************************************************************/

/******************************************************************************/
boolean checkButtons(uint32_t ms)
{
  boolean pb = false;
  boolean mb = false;
  boolean sb = false;
  boolean setRTC = false;

  if (LOW == digitalRead(PIN_BTN_UP))   pb = true;
  if (LOW == digitalRead(PIN_BTN_DOWN)) mb = true;
  if (LOW == digitalRead(PIN_BTN_SET))  sb = true;

  if ((pb == false) && (mb == false) && (sb == false)) {
    // no buttons were pressed.
    // If we are not in TIME mode and sufficient time has passed,
    // switch back to TIME mode
    if ((mode != MENU_TIME) && ((long)(ms - timeLeaveMenu) >= 0)) {
      mode = MENU_TIME;
      getTimeFromRTC(100);
      dispMode(ms);
    }
    return false;
  }

#ifdef _DEBUG_
  Serial.print(" SB = "); Serial.print(sb);
  Serial.print(" PB = "); Serial.print(pb);
  Serial.print(" MB = "); Serial.println(mb);
#endif

  // some button was pressed, silence the alarm
  if (alSounding) {
    silenceAlarm();
    alSounding = false;
    return false;
  }

  if (sb == true) {	// advance to the next menu mode
#ifdef _DEBUG_
    Serial.println("set button pressed");
#endif
    mode++;
#ifdef WANT_ESP8266
    if (mode == MENU_WIFI_INIT && wifiOn != 2) mode = MENU_DST_ENABLE;
#endif
    if (mode == MENU_DST_START_WEEK && doDST == 0) mode = MENU_TIME;
    dispMode(ms);		// update display for appropriate menu item
    timeLeaveMenu = ms + 15000;  // switch back to TIME mode in 15s
    return true;
  }

  switch (mode) {
    case MENU_ALARM_ON:			// menu alarm enable/disable
      alOn = alOn ? false : true;
      eeprom_write_byte((uint8_t *)EEPROM_ALARM_ON, alOn);
      break;

    case MENU_ALARM_HOUR:		// menu alarm hour
      if (pb) {
	if (++alHr > 23) alHr = 0;
      } else {
	if (--alHr < 0) alHr = 23;
      }
      eeprom_write_byte((uint8_t *)EEPROM_ALARM_HR, alHr);
      break;

    case MENU_ALARM_MINUTE:		// menu alarm minute
      if (pb) {
	if (++alMn > 59) alMn = 0;
      } else {
	if (--alMn < 0) alMn = 59;
      }
      eeprom_write_byte((uint8_t *)EEPROM_ALARM_MN, alMn);
      break;

    case MENU_SET_HOUR:			// menu set hour
      if (pb) {
	if (++utcTm.Hour > 23) utcTm.Hour = 0;
      } else {
	if (--utcTm.Hour < 0) utcTm.Hour = 23;
      }
      setTime(&utcTm);
      setRTC = true;
      break;

    case MENU_SET_MINUTE:		// menu set minute
      if (pb) {
	if (++utcTm.Minute > 59) utcTm.Minute = 0;
      } else {
	if (--utcTm.Minute < 0) utcTm.Minute = 59;
      }
      setTime(&utcTm);
      setRTC = true;
      break;

    case MENU_SET_SECOND:		// menu set second
      if (pb) {
	if (++utcTm.Second > 59) utcTm.Second = 0;
      } else {
	if (--utcTm.Second < 0) utcTm.Second = 59;
      }
      setTime(&utcTm);
      setRTC = true;
      break;

    case MENU_12HOUR:			// menu 12 Hour/24 Hour select
      do12Hr = do12Hr ? 0 : 1;
      eeprom_write_byte((uint8_t *)EEPROM_12HOUR, do12Hr);
      break;

    case MENU_SCROLL:			// menu scroll speed
       if (mb) scrollSpeed--;
       else scrollSpeed++;
       if (scrollSpeed > 9) scrollSpeed = 0;
       eeprom_write_byte((uint8_t *)EEPROM_SCROLL, scrollSpeed);
       break;

    case MENU_BRIGHT:			// menu display brightness
      if (mb) {				// brite 0 == max, 7 == off
	brite++;
	if (brite >= HDSP253X_BRIGHT_0)
	  brite = HDSP253X_BRIGHT_100;
      } else {
	brite--;
	if (brite >= HDSP253X_BRIGHT_0)
	  brite = HDSP253X_BRIGHT_13;
      }
      setBrightness();
      eeprom_write_byte((uint8_t *)EEPROM_BRIGHT, brite);
      break;

    case MENU_CHIMES:			// menu enable/disable chimes
      doChimes = doChimes ? 0 : 1;
      eeprom_write_byte((uint8_t *)EEPROM_CHIMES, doChimes);
      break;

#ifdef WANT_PROVERBS
    case MENU_PROVERBS:
      doProverbs = doProverbs ? 0 : 1;
      eeprom_write_byte((uint8_t *)EEPROM_PROVERBS, doProverbs);
      break;
#endif

#ifdef WANT_MOONPHASE
    case MENU_MOONPHASE:
      doMoon = doMoon ? 0 : 1;
      eeprom_write_byte((uint8_t *)EEPROM_MOONPHASE, doMoon);
      break;
#endif

    case MENU_YEAR:			// menu year
      if (mb) {
	if (--utcTm.Year > 99 ) utcTm.Year = 0;
      } else {
	if (++utcTm.Year > 99) utcTm.Year = 0;
      }
      utcTm.Wday = zellersDow(&utcTm);
      setDate(&utcTm);
      setRTC = true;
      break;

    case MENU_MONTH:			// menu month
      if (mb) {
	if (--utcTm.Month < 1) utcTm.Month = 12;
      } else {
	if (++utcTm.Month > 12) utcTm.Month = 1;
      }
      utcTm.Wday = zellersDow(&utcTm);
      setDate(&utcTm);
      setRTC = true;
      break;

    case MENU_DAY:			// menu day
      if (mb) {
	if (--utcTm.Day < 1) utcTm.Day = 28;
      } else {
	if (++utcTm.Day > 31) utcTm.Day = 1;
      }
      utcTm.Wday = zellersDow(&utcTm);
      setDate(&utcTm);
      setRTC = true;
      break;

    case MENU_TZ_HOUR:
      if (mb) {
        if (--TZhr < -12) TZhr = 14;
      } else {
        if (++TZhr > 14) TZhr = -12;
      }
      eeprom_write_byte((uint8_t *)EEPROM_TZ_HOUR, TZhr);
      setTzRule();
      break;

    case MENU_TZ_MINUTE:
      if (mb) {
	TZmn -= 15;
        if (TZmn < -45) TZmn = 0;
      } else {
	TZmn += 15;
        if (TZmn > 45) TZmn = 0;
      }
      eeprom_write_byte((uint8_t *)EEPROM_TZ_MINUTE, TZmn/15);
      setTzRule();
      break;

#ifdef WANT_ESP8266
    case MENU_WIFI_ON:			// menu WIFI enable/disable/init
      if (mb) {
	if (--wifiOn > 1) wifiOn = 1;
      } else {
	if (++wifiOn > 2) wifiOn = 0;
      }
      if (wifiOn < 2)
	eeprom_write_byte((uint8_t *)EEPROM_WIFI_ON, wifiOn);
      if (wifiOn == 1) esp8266.wifiOn();
      else if (wifiOn == 0) esp8266.wifiOff();

      break;

    case MENU_WIFI_INIT:
      if (mb) {
	if (--wifiOn < 2) wifiOn = 3;
      } else {
	if (++wifiOn > 3) wifiOn = 2;
      }
      if (wifiOn == 3) esp8266.wifiInit();
      break;
#endif

    case MENU_DST_ENABLE:		// Daylight savings enable
      doDST = doDST ? 0 : 1;
      eeprom_write_byte((uint8_t *)EEPROM_DST_ENABLE, doDST);
      setTzRule();
      break;

    case MENU_DST_START_MONTH:		// Starting month of Daylight Savings
      if (mb) {
	if (--DSTsm < 1) DSTsm = 12;
      } else {
	if (++DSTsm > 12) DSTsm = 1;
      }
      eeprom_write_byte((uint8_t *)EEPROM_DST_SM, DSTsm);
      setTzRule();
      break;

    case MENU_DST_START_WEEK:		// Starting week of Daylight Savings
      if (mb) {
	if (--DSTsw < 1) DSTsw = 4;
      } else {
	if (++DSTsw > 4) DSTsw = 1;
      }
      eeprom_write_byte((uint8_t *)EEPROM_DST_SW, DSTsw);
      setTzRule();
      break;

    case MENU_DST_END_MONTH:		// Ending month of Daylight Savings
      if (mb) {
	if (--DSTem < 1) DSTem = 12;
      } else {
	if (++DSTem > 12) DSTem = 1;
      }
      setTzRule();
      eeprom_write_byte((uint8_t *)EEPROM_DST_EM, DSTem);
      break;

    case MENU_DST_END_WEEK:		// Ending week of Daylight Savings
      if (mb) {
	if (--DSTew < 1) DSTew = 4;
      } else {
	if (++DSTew > 4) DSTew = 1;
      }
      setTzRule();
      eeprom_write_byte((uint8_t *)EEPROM_DST_EW, DSTew);
      break;

    default:
      mode = MENU_TIME;
      break;
  }
  if (setRTC) getTimeFromRTC(100);
  dispMode(ms);			// update display for appropriate menu item
  if (mode != MENU_TIME) timeLeaveMenu = ms + 15000;  // switch back to TIME mode in 15s
  return true;
}
/******************************************************************************/

/******************************************************************************/
// Main program loop

void loop()
{
  uint32_t ms = millis();

#ifdef WANT_ESP8266
  esp8266.check(ms);
  if (esp8266.sntpValid) {
    esp8266.sntpValid = false;
    if (utcTt != esp8266.sntpTt) {
      utcTt = esp8266.sntpTt;
      breakTime(utcTt, &utcTm);
      setTime(&utcTm);
      setDate(&utcTm);
      locTt = toLocal(utcTt);
      breakTime(locTt, &locTm);
    }
  }
#endif
#ifdef WANT_GPS
  gps.check(ms);
  if (gps.doSync) {
    gps.doSync = false;
    if ((gps.tme.Second != utcTm.Second)
    ||  (gps.tme.Minute != utcTm.Minute)
    ||  (gps.tme.Hour != utcTm.Hour)
    ||  (gps.tme.Day != utcTm.Day)
    ||  (gps.tme.Month != utcTm.Month)
    ||  (gps.tme.Year != utcTm.Year)
    ) {
      gps.tme.Wday = zellersDow(&gps.tme);
      utcTm = gps.tme;
      setTime(&utcTm);
      utcTt = makeTime(&utcTm);
      locTt = toLocal(utcTt);
      breakTime(locTt, &locTm);
    }
  }
#endif
#ifdef WANT_BLUETOOTH
  checkBlueTooth();
#endif

  if (isScrolling && ((long)(ms - timeNextScroll) >= 0)) {
    doScroll(ms);
  }

  if (alSounding && ((long)(ms - timeAlarmStop) >= 0)) {
    silenceAlarm();
    alSounding = false;
  }

  checkSound(ms, alSounding);

  if (isFlashing && ((long)(ms - timeNextFlash) >= 0)) {
    timeNextFlash = ms + 128;
    dispMode(ms);
  }

  if ((long)(ms - timeNextButton) >= 0) {
    if (checkButtons(ms)) {
      timeNextButton = ms + 400;	// button pressed, lock out further presses (debounce)
      isScrolling = false;
    } else {
      timeNextButton = ms + 100;	// button not pressed, no need to read RTC very often
    }

    getTimeFromRTC(lastSecond);
    if (lastSecond != locTm.Second) {
      lastSecond = locTm.Second;
      if ((mode == MENU_TIME) && alOn
      &&  (locTm.Hour == alHr) && (locTm.Minute == alMn) && (locTm.Second == 0)) {
	if (!alSounding) {
	  alSounding = true;
	  soundAlarm();
	  timeAlarmStop = ms + 60000;
	}
      }
      if (doChimes && (locTm.Second == 0)) {
	if (locTm.Minute == 0) {
	  soundChimeLong();
	} else if (locTm.Minute == 30) {
	  soundChimeShort();
	}
      }
      if (!isScrolling) {
	dispMode(ms);
	if (mode == MENU_TIME) {
	  // display a message every minute;
	  if ((locTm.Second >= 5) && (locTm.Second < 45)
	  &&  ((long)(ms - timeNextDate) >= 0)) {
	    boolean found;

	    memset(tempBuffer, ' ', 8);
	    found = false;
	    while(!found) {
	      if (++dispType > 3) dispType = 0;

	      if (dispType == 0) {
		getCurrentDate(&locTm, &tempBuffer[8]);
		found = true;
#ifdef WANT_PROVERBS
	      } else if (dispType == 1) {
		/* Proverbs */
		if (doProverbs) {
		  getProverb(&tempBuffer[8], sizeof(tempBuffer)-8);
		  found = true;
		}
#endif
	      } else if (dispType == 2) {
#ifdef WANT_ESP8266
		if (esp8266.rssValid) {
		  esp8266.getMsg(&tempBuffer[8], sizeof(tempBuffer)-8);
		  if (esp8266.dispOnce) {
		    esp8266.dispOnce = false;
		    esp8266.rssValid = false;
		  }
		  found = true;
		}
#endif
#ifdef WANT_BLUETOOTH
		if (bt_valid) {
		  BTgetMsg(&tempBuffer[8], sizeof(tempBuffer)-8);
		  found = true;
		}
#endif
	      } else if (dispType == 3) {
#ifdef WANT_MOONPHASE
		if (doMoon) {
		  getCurrentMoon(utcTt, &tempBuffer[8]);
		  found = true;
		}
#endif
	      }
	    }
	    scrollDisplay();
	  }
	}
      }
    }
  }
}
/******************************************************************************/
