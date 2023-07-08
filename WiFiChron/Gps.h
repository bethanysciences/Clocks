// Gps.h

#include "UserConf.h"
#ifdef WANT_GPS
#ifndef _GPS_H_
#define _GPS_H_

#if ARDUINO < 100
  #include <WProgram.h>
#else
  #include <Arduino.h>
#endif
#include <stdlib.h>
#include "TimeLib.h"

class CGps
{
public:		// public storage
    tmElements_t tme;		/* GPS time */
    boolean synced;		/* GPS time has been synced */
    boolean doSync;		/* Can sync RTC with GPS */
private:	// private storage
    char buff[64];		/* GPS NMEA command buffer */
    boolean valid;		/* GPS time is valid */
    uint8_t csum;		/* GPS input calculated check sum */
    uint8_t asum;		/* GPS supplied check sum */
    uint8_t op;			/* NMEA command read */
    uint8_t state;		/* input state machine state */
    uint8_t indx;		/* input buffer point */
    uint32_t nextSync;		/* when to sync RTC with GPS */
    uint32_t notSync;		/* when not sync'd with GPS */


public:		// public functions
    void init();		/* initialization -- called once */
    void check(uint32_t ms);	/* look for input from GPS */

private:	// private functions
    void start();		/* init for each line of input */
    void cmd(uint32_t ms);	/* process a line from GPS */
};

extern CGps gps;
#endif	// _GPS_H_
#endif	// WANT_GPS
