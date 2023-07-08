#include "UserConf.h"
#ifdef WANT_GPS
#include <Arduino.h>
#include "Gps.h"
#include <stdlib.h>
#include "TimeLib.h"

/*
 * GPS routines
 */

#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  // set the serial interface to Serial1
  #define Gps_Port Serial1
#else
  // set the serial interface to Serial (only one on 328P)
  #define Gps_Port Serial
#endif

void CGps::start()
{
  csum = 0;
  asum = 255;
  op = 0;
  state = 0;
  indx = 0;
}

void CGps::init()
{
  tme.Second = 0;
  tme.Minute = 0;
  tme.Hour = 0;
  tme.Wday = 7;
  tme.Day = 1;
  tme.Month = 1;
  tme.Year = 0;
  Gps_Port.begin(9600);
  start();
  valid = false;
  synced = false;
  doSync = false;
  /* turn off anoying report about no antenna */
  Gps_Port.println("$PGCMD,33,0*6D");
  delay(100);
  /* get a "fix" every second (1000ms) */
  Gps_Port.println("$PMTK300,1000,0,0,0,0*1C");
  delay(100);
  /* report GPRMC and GPZDA every five "fixes" */
  Gps_Port.println("$PMTK314,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0*28");
}

/* Process the GPS report (GPZDA or GPRMC) */
void CGps::cmd(uint32_t ms)
{
  int8_t i, j;
  int16_t yr;

  if (op == 1) {
    valid = false;		/* mark as false until we know otherwise */
    doSync = false;
    /* GPRMC result - time, status, position, date */
    /* $GPRMC,224532,A,Latitude,north/south,Longitude,East/West,speed,course,140516,variation,east/west */
    /* find first comma */
    i = 6;
    while((buff[i] != 0) && (buff[i] != ','))
      i++;
    // first field after time is status. 'A' means valid sample
    if ((buff[i] == ',') && (buff[i+1] == 'A')) {
      tme.Hour   = (buff[0] - '0') * 10 + buff[1] - '0';
      tme.Minute = (buff[2] - '0') * 10 + buff[3] - '0';
      tme.Second = (buff[4] - '0') * 10 + buff[5] - '0';
      i++;	// buff[i] should now be 'A'
      j = 7;	// skip the next seven fields (including status field)
      while(buff[i] != 0) {
	if (buff[i++] == ',') {
	  if (--j == 0) {	/* reached date info */
	    tme.Day   = (buff[i] - '0') * 10 + buff[i+1] - '0';
	    i += 2;
	    tme.Month = (buff[i] - '0') * 10 + buff[i+1] - '0';
	    i += 2;
	    tme.Year  = (buff[i] - '0') * 10 + buff[i+1] - '0';
	    valid = true;
	    break;
	  }
	}
      }
    }
  } else if (op == 2) {
    /* GPZDA result - time and date */
    /* $GPZDA,224532.00,14,11,2014,... */
    if (!valid) {	/* only trust GPZDA if we've seen a valid GPRMC */
      op = 0;
      return;
    }
    /* valid GPZDA result */
    tme.Hour   = (buff[0] - '0') * 10 + buff[1] - '0';
    tme.Minute = (buff[2] - '0') * 10 + buff[3] - '0';
    tme.Second = (buff[4] - '0') * 10 + buff[5] - '0';
    i = 6;
    while(buff[i] != 0) {
      if (buff[i++] == ',') {
	tme.Day = (buff[i] - '0') * 10 + buff[i+1] - '0';
	i += 2;
	break;
      }
    }
    while(buff[i] != 0) {
      if (buff[i++] == ',') {
	tme.Month = (buff[i] - '0') * 10 + buff[i+1] - '0';
	i += 2;
	break;
      }
    }
    if (buff[i++] == ',') {
      yr = 0;
      while((buff[i] >= '0') && (buff[i] <= '9')) {
	yr *= 10;
	yr += buff[i++] - '0';
      }
      if (yr > 100) yr %= 100;
	tme.Year = yr;		// normalized to years since 2000
    }
  } else {
    op = 0;
    return;
  }

  /* handled the two types of incoming data */
  if (!valid) {			// return if we don't have valid data
    op = 0;
    return;
  }

  /* Since data is valid, sync the clock */
  if ((!synced) || ((long)(ms - nextSync) >= 0)) {
    nextSync = ms + 60000;		// sync only once a minute
    notSync = ms + 900000;		// not sync'd fifteen minutes later
    synced = true;
    doSync = true;
  }

#ifdef _DEBUG_
  Serial.println();
  sprintf_P(buff, PSTR("GPS %02d:%02d:%02d"), tme.Hour, tme.Minute, tme.Second);
  Serial.println(buff);
  sprintf_P(buff, PSTR("GPS %02d/%02d/%02d"), tme.Month, tme.Day, tme.Year);
  Serial.println(buff);
#endif
  if (op == 2) {
    valid = false;	/* wait for another GPRMC before trusting GPZDA */
    doSync = false;
  }
  op = 0;
}

/* Consume any input from the GPS */
void CGps::check(uint32_t ms)
{
  char c;

  while(Gps_Port.available() > 0) {
    c = Gps_Port.read();
#ifdef _DEBUG_
      Serial.write(c);
#endif
    if (c == '\r' || c == '\n') {
      if (asum == csum) {
	cmd(ms);
      }
      start();
    }
    if (c == '$') {
      start();
    } else if (c == '*') {
      state = 2;
      asum = 0;
    } else if (state == 2) {
      asum <<= 4;
      asum |= c - '0';
    } else {
      csum ^= c;
      if (indx < (sizeof(buff) - 2)) {
	buff[indx++] = c;
	buff[indx] = 0;
      }
      if ((state == 0) && (c == ',')) {
	state = 1;
	if (strcmp_P(buff, PSTR("GPRMC,")) == 0) {
	  op = 1;
	  indx = 0;
	} else if (strcmp_P(buff, PSTR("GPZDA,")) == 0) {
	  op = 2;
	  indx = 0;
	}
      }
    }
  }
  if (synced && ((long)(ms - notSync) >= 0)) {
    synced = false;
    doSync = false;
  }
}
//
// End of GPS routines
//
CGps gps;
#endif // WANT_GPS
