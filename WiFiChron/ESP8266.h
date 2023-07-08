// ESP8266.h

//*************************************************************************
//*	Edit History, started September, 2012
//*	please put your initials and comments here anytime you make changes
//*************************************************************************
//* Feb 22/15 (mcm)  initial version
//*************************************************************************

#include "UserConf.h"
#ifdef WANT_ESP8266
#ifndef _ESP8266_H_
#define _ESP8266_H_

#if ARDUINO < 100
  #include <WProgram.h>
#else
  #include <Arduino.h>
#endif

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>

#define EEPROM_SSID	32
#define EEPROM_PWD	66
#define EEPROM_SNTP	132
#define EEPROM_RSS	198

class CEsp8266
{
public:		// public storage
    uint32_t sntpTt;		// SNTP timestamp (seconds from Jan 1, 2000)
    boolean sntpValid;		// SNTP values are valid
    boolean sntpSynced;		// received SNTP packet
    boolean rssValid;		// RSS message buffer is valid
    boolean dispOnce;		// display the RSS message only once

private:	// private storage
    uint32_t timer;		// command timer value
    uint32_t timeInputDone;	// when to timeout on input
    uint32_t timeNextRss;	// when to fire RSS
    uint32_t timeCloseRss;	// when to close the RSS connection
    uint32_t timeNextSntp;	// when to fire SNTP
    uint32_t timeNotSntpSynced;	// when to invalidate SNTP sync
    uint32_t timeCloseSntp;	// when to close the SNTP connection
    int16_t cfgPos;		// where to write config info
    int16_t rPos;		// RSS message input position
    int16_t orPos;		// RSS message output position
    int16_t dataLen;		// incoming data length
    boolean timerActive;	// event timer is active
    boolean wantReady;		// looking for "ready" instead of "OK"
    int8_t  sentBaud;		// sent Baud-rate command
    boolean sawBusy;		// saw "busy" instead of "OK"
    boolean isJoined;		// has joined an AP
    boolean dataIn;		// receiving data, not command status
    boolean webCmdDone;		// received all of web server command
    boolean cfgDone;		// received cfg command
    boolean doRss;		// RSS has been configured
    boolean doSntp;		// SNTP has been configured
    boolean inApMode;		// local AP mode
    boolean webCfgMode;		// parsing web server configuration request
    int8_t  mPos;		// message position
    uint8_t state;		// current state
    uint8_t webState;		// state of parsing web server request
    uint8_t nextState;		// when command sequence finishes, enter this state
    uint8_t rstNext;		// next state after reset
    int8_t  rstCnt;		// max consecutive resets
    uint8_t errCnt;		// number of errors before requiring a reset
    uint8_t busyCnt;		// number of busys before trying to re-connect
    uint8_t cipStat;		// connection status
    uint8_t rssStat;		// RSS message state
    int8_t  rssRetry;		// RSS retry counter
    uint8_t pktCid;		// packet's channel ID
    uint8_t sntpStat;		// SNTP state
    uint8_t msgBuf[64];		// message buffer
    uint8_t rssBuf[512];	// RSS messages
    uint8_t extBuf[16];		// last part of un-processed packet


public:		// public functions
    void init();				// initialize serial port
    void wifiOff();				// disconnect from AP
    void wifiOn();				// join AP
    void wifiInit();				// start in AP mode for config
    void check(uint32_t ms);			// look for messages
    void getMsg(char *buf, int8_t sz);		// get recent RSS for scroll
    void nextMsgPart(char *buf, int8_t sz);	// get next part of RSS scroll

private:	// private functions
    void sendCfgWebPage();			// send configuration web page
    void wifiStart();				// initialize internal state
    void updRss(uint8_t chr, int8_t cnt, uint8_t stat, boolean flg);
    void writeCfgToEEprom();			// write saved values to eeprom
};

extern CEsp8266 esp8266;

#endif	// _ESP8266_H_
#endif	// WANT_ESP8266
