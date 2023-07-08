// ESP8266.cpp

//*************************************************************************
//*	Edit History, started February, 2015
//*	please put your initials and comments here anytime you make changes
//*************************************************************************
//* Feb 22/15 (mcm)  initial version
//* Feb 10/16 (mcm)  added User-Agent and Accept headers on RSS requests
//* Apr 08/16 (mcm)  Revamped RSS state machine to be more CPU efficient
//* May 14/16 (mcm)  Changes for 1284 processor
//*************************************************************************
#include "UserConf.h"
#ifdef WANT_ESP8266

#include <Arduino.h>
#include "ESP8266.h"
#include <stdlib.h>

#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
  // set the serial interface to Serial1
  #define Esp_Port Serial1
#else
  // set the serial interface to Serial (only one on 328P)
  #define Esp_Port Serial
#endif

//#define DEBUG
//#define DEBUG_SNTP
//#define DEBUG_CMD

enum				// WiFi module states
{
  ESP8266_NOOP = 0,		// no operation
  ESP8266_AT,			// See if module is operational
  ESP8266_UART,			// set the baud rate
  ESP8266_CWMODE1,		// set station mode
  ESP8266_RST,			// issue a reset
  ESP8266_CIPMUX,		// allow multiple connections
  ESP8266_CWJAP,		// Join an AP
  ESP8266_CIFSR,		// test if join succeeded
  ESP8266_ATE0,			// disable command echo
  ESP8266_CIPSTART_RSS,		// connect to RSS service
  ESP8266_CIPSTATUS_RSS,	// check RSS connection status
  ESP8266_CIPSEND_RSS,		// send RSS request
  ESP8266_CIPCLOSE_RSS,		// close RSS connection
  ESP8266_CIPSTART_SNTP,	// connect to SNTP service
  ESP8266_CIPSEND_SNTP,		// send SNTP request
  ESP8266_CIPCLOSE_SNTP,	// close SNTP connection
  ESP8266_CWMODE2,		// change to AP mode
  ESP8266_CWSAP,		// set ssid, pwd for AP mode
  ESP8266_CIPSERVER_START,	// start web service
  ESP8266_CIPSEND_SERVER,	// send web response
  ESP8266_CIPCLOSE_SERVER,	// close server connection
  ESP8266_CIPSERVER_STOP,	// stop web service
  ESP8266_CWQAP,		// disconnect from AP
};

int8_t bindx = 0;
int32_t bauds[4] = { 38400, 115200, 9600, 57600 };

//
// initialize WiFi state
//
void CEsp8266::wifiStart()
{
  timerActive = false;
  sntpValid = false;
  sntpSynced = false;
#ifdef DEBUG
  rssValid = true;
#else
  rssValid = false;
#endif
  wantReady = false;
  sentBaud = 0;
  sawBusy = false;
  isJoined = false;
  doRss = false;
  doSntp = false;
  mPos = 0;
  rPos = 0;
  orPos = 0;
  msgBuf[0] = 0;
  rssBuf[0] = 0;
  extBuf[0] = 0;
  webCfgMode = false;
  cfgDone = false;
  cfgPos = 0;
  inApMode = false;
  sntpStat = 0;
  dispOnce = false;
  errCnt = 0;
  busyCnt = 0;
}

//
// initialize the serial interface to the WiFi module
//
void CEsp8266::init()
{
  Esp_Port.begin(bauds[bindx]);
  wifiStart();
}

//
// disconnect the WiFi
//
void CEsp8266::wifiOff()
{
  wifiStart();
  state = ESP8266_CWQAP;		// disconnect from AP
}

//
// initialize the WiFi Configuration information (AP mode)
//
void CEsp8266::wifiInit()
{
  wifiStart();
  state = ESP8266_CWMODE2;		// AP mode, then start web service
}

//
// connect the WiFi to the configured AP
//
void CEsp8266::wifiOn()
{
  wifiStart();
  state = ESP8266_AT;
}

const char Web1[] PROGMEM =
"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-length: ";
const char Web2[] PROGMEM = "<html><body><H1>WiFiChron Setup Page</H1><br><form action=\"/\" method=\"post\"><table><tr><td>Wireless SSID</td><td><input type=\"text\" name=\"ssid\" value=\"";
const char Web3[] PROGMEM =
"\"></td></tr><tr><td>Wireless Password</td><td><input type=\"text\" name=\"pwd\" value=\"";
const char Web4[] PROGMEM =
"\"></td></tr><tr><td>SNTP Server</td><td><input type=\"text\" name=\"sntp\" value=\"";
const char Web5[] PROGMEM =
"\"></td></tr><tr><td>RSS URL</td><td><input type=\"text\" name=\"rss\" value=\"";
const char Web6[] PROGMEM =
"\"></td></tr><tr><td>Activate</td><td><input type=\"checkbox\" name=\"act\" value=\"1\"></td></tr></table><br><input type=\"hidden\" name=\"f\" value=\"1\"><input type=\"submit\" value=\"Submit\"></form></body></html>\r\n";

//
// send configuration web page
//
void CEsp8266::sendCfgWebPage()
{
  uint8_t *cp;
  uint8_t c;
  int16_t len;

  len = strlen_P(Web2) + strlen_P(Web3) + strlen_P(Web4)
      + strlen_P(Web5) + strlen_P(Web6);

  cp = (uint8_t *)EEPROM_SSID;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    len++;
  }
  cp = (uint8_t *)EEPROM_PWD;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    len++;
  }
  cp = (uint8_t *)EEPROM_SNTP;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    len++;
  }
  cp = (uint8_t *)EEPROM_RSS;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    len++;
  }
  sprintf_P((char *)msgBuf, PSTR("%d\r\n\r\n"), len);
  mPos = 0;
  len += strlen_P(Web1);
  len += strlen((char *)msgBuf);
  Esp_Port.print(F("AT+CIPSEND="));
  Esp_Port.write(pktCid);
  Esp_Port.print(F(","));
  Esp_Port.print(len);
  Esp_Port.print(F("\r\n"));
  delay(5);
  cp = (uint8_t *)Web1;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
  Esp_Port.print((char *)msgBuf);
  cp = (uint8_t *)Web2;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)EEPROM_SSID;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)Web3;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)EEPROM_PWD;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)Web4;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)EEPROM_SNTP;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)Web5;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)EEPROM_RSS;
  while(((c = eeprom_read_byte(cp++)) != 0) && (c < 127)) {
    Esp_Port.write(c);
  }
  cp = (uint8_t *)Web6;
  while((c = pgm_read_byte(cp++)) != 0) {
    Esp_Port.write(c);
  }
}

/*
 * Note -- EEPROM write cycles are very long (~3ms), so writing
 * to EEPROM while receiving the packet will cause buffer over-runs.
 * We have to buffer the EEPROM writes in RAM, then write to
 * EEPROM after the data has been processed.
 */
#define RSSbuf_SSID	1	// offset for SSID (32 + null)
#define RSSbuf_PWD	34	// offset for PWD (64 + null)
#define RSSbuf_SNTP	99	// offset for SNTP (128 + null)
#define RSSbuf_RSS	228	// offset for RSS URL

void CEsp8266::writeCfgToEEprom()
{
  uint8_t *inp, *eep;
  uint8_t c;

  inp = &rssBuf[RSSbuf_SSID];
  if (*inp) {
    eep = (uint8_t *)EEPROM_SSID;
    do {
      c = *inp++;
      eeprom_write_byte(eep++, c);
    } while((c != 0) && (inp < &rssBuf[RSSbuf_PWD]));
  }
  inp = &rssBuf[RSSbuf_PWD];
  if (*inp) {
    eep = (uint8_t *)EEPROM_PWD;
    do {
      c = *inp++;
      eeprom_write_byte(eep++, c);
    } while((c != 0) && (inp < &rssBuf[RSSbuf_SNTP]));
  }
  inp = &rssBuf[RSSbuf_SNTP];
  if (*inp) {
    eep = (uint8_t *)EEPROM_SNTP;
    do {
      c = *inp++;
      eeprom_write_byte(eep++, c);
    } while((c != 0) && (inp < &rssBuf[RSSbuf_RSS]));
  }
  inp = &rssBuf[RSSbuf_RSS];
  if (*inp) {
    eep = (uint8_t *)EEPROM_RSS;
    do {
      c = *inp++;
      eeprom_write_byte(eep++, c);
    } while((c != 0) && (inp < &rssBuf[RSSbuf_RSS+200]));
  }
  rPos = 0;
  rssBuf[0] = 0;
  rssValid = false;
}

void CEsp8266::updRss(uint8_t chr, int8_t cnt, uint8_t stat, boolean flg)
{
  rssStat = stat;
  rssValid = flg;
  if ((chr != 0) && (rPos < (sizeof(rssBuf) - 2))) {
    if ((chr < ' ') || (chr >= 127)) chr = ' ';
    if ((chr != ' ') || (rPos == 0) || (rssBuf[rPos-1] != ' ')) {
      rssBuf[rPos++] = chr;
      rssBuf[rPos] = 0;
    }
  }
  if (cnt > 0) {
    mPos -= cnt;
    memmove(msgBuf, &msgBuf[cnt], mPos+1);
  }
}

void CEsp8266::check(uint32_t ms)
{
  uint8_t c;
  uint8_t *cp;

  while(Esp_Port.available() > 0) {
    c = Esp_Port.read();
    if (dataIn) {
      // reading data packet, not command
      if (--dataLen <= 0) dataIn = false;
      if (mPos >= (sizeof(msgBuf) - 2)) {
	memmove(msgBuf, &msgBuf[sizeof(msgBuf)/2], sizeof(msgBuf)/2);
	mPos -= sizeof(msgBuf) / 2;
      }
      msgBuf[mPos++] = c;
      msgBuf[mPos] = 0;

      if (sntpStat == 3) {		// receiving SNTP packet
	if (!dataIn) {			// packet ended
	  uint32_t ts;
	  int8_t i;
	  ts = 0;
	  for(i=40; i<=43; i++) {
	    ts *= 256;
	    ts |= msgBuf[i];
	  }
#ifdef DEBUG_SNTP
	  rPos = 0;
	  sprintf_P((char *)&rssBuf[rPos], PSTR(" sntp - %02x.%02x.%02x.%02x - %08lx "),
		msgBuf[40],msgBuf[41],msgBuf[42],msgBuf[43], ts);
	  rssValid = true;
	  rPos += strlen((char *)&rssBuf[rPos]);
#endif
	  if (ts != 0) {			// valid stamp
	    // SNTP timestamp is the number of seconds since Jan 1, 1900.
	    // We're keeping time since Jan 1, 2000.
	    // There were 3,155,673,600 seconds between 1900 and 2000.
	    // convert SNTP stamp to offset from 2000 by subtracting 3155673600.
	    // Should we also subtract leap seconds, 26 of them so far ????
	    ts -= 3155673600;
	    sntpTt = ts;
	    sntpSynced = true;
	    timeNotSntpSynced = ms + 7200000;
	    sntpValid = true;
	    strcpy_P((char *)rssBuf, PSTR("Time synced"));
	    rPos = 11;
	    rssValid = true;
	    dispOnce = true;
	  }
	  state = ESP8266_CIPCLOSE_SNTP;
	  rssRetry = 5;
	}
      } else if (rssStat >= 2) {	// receiving RSS packet
	switch(rssStat) {
	  case 2:	// look for <item> or </rss>
	    if (c == '<') 		// wait for initial '<'
	      rssStat = 3;
	    else
	      mPos = 0;
	    break;

	  case 3:	// found opening '<', buffer 6 chars
	    if (c == '<') {	// start over if another '<' is found
	      mPos = 1;
	    } else if (mPos == 6) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("<item>")) == 0) {
		rssStat = 4;
		mPos = 0;
	      } else if (strcasecmp_P((char *)msgBuf, PSTR("</rss>")) == 0) {
		// packet ended
		rssStat = 11;
		mPos = 0;
	      } else {
		rssStat = 2;		// go back to looking for '<'
		mPos = 0;
	      }
	    }
	    break;

	  case 4:	// look for '<' to start <title> or <description>
	    if (c == '<') 
	      rssStat = 5;
	    else
	      mPos = 0;
	    break;

	  case 5:	// found opening '<', check for <title> or <description>
	    if (c == '<') {	// start over if another '<' is found
	      mPos = 1;
	    } else if (mPos == 6) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("</rss>")) == 0) {
		// packet ended
		rssStat = 11;
		mPos = 0;
	      }
	    } else if (mPos == 7) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("<title>")) == 0) {
		rssStat = 6;		// start saving RSS data
		mPos = 0;
	      }
	    } else if (mPos == 13) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("<description>")) == 0) {
		rssStat = 6;		// start saving RSS data
	      } else {
		rssStat = 4;		// go back to looking for '<'
	      }
	      mPos = 0;
	    }
	    break;

	  case 6:
	    // incoming data for the rss buffer
	    // add it to the rss buffer directly until a '<' is found
	    // just queue it if we see a '<'
	    if (c == '<') {
	      rssStat = 7;
	    } else if (c == '&') {
	      rssStat = 12;
	    } else {
	      updRss(c, 1, 6, rssValid);
	    }
	    break;

	  case 7:
	    // incoming data for the rss buffer
	    // but saw a '<', so buffer to check it for HTML tags
	    if (mPos == 6) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("</rss>")) == 0) {
		// packet ended
		updRss(0, 6, 11, rssValid);
	      }
	    } else if (mPos == 8) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("</title>")) == 0) {
		updRss(' ', 8, 4, true);
	      }
	    } else if (mPos == 9) {
	      if (strcmp_P((char *)msgBuf, PSTR("<![CDATA[")) == 0) {
		updRss(0, 9, 8, rssValid);
	      }
	    } else if (mPos == 14) {
	       if (strcasecmp_P((char *)msgBuf, PSTR("</description>")) == 0) {
		updRss(' ', 14, 2, true);
	      } else {
		updRss('<', 1, 6, rssValid);
		while((mPos != 0) && (msgBuf[0] != '<')) {
		  updRss(msgBuf[0], 1, 6, rssValid);
		}
		if (mPos != 0) rssStat = 7;
	      }
	    }
	    break;

	  case 8:	// saw <!CDATA[ tag, look for the ending ']]>'
	    if (c == ']')	// saw first ']'
	      rssStat = 9;
	    mPos = 0;
	    break;

	  case 9:
	    if (c == ']') // saw second ']'
	      rssStat = 10;
	    else
	      rssStat = 8; // didn't see second ']', look for starting ']'
	    mPos = 0;
	  break;

	  case 10:
	    if (c == '>')	// saw ending '>', start saving RSS data again
	      rssStat = 6;
	    else if (c != ']')	// didn't see ending '>', but maybe saw ']]]'
	      rssStat = 8;	// start over looking for ']]>'
	    mPos = 0;
	  break;

	  case 11:
	    // saw </rss>, so wait for end of data then close the connection
	    if (!dataIn)
	      state = ESP8266_CIPCLOSE_RSS;
	    mPos = 0;
	    break;

	  case 12:
	    // saw '&', so wait for a few characters to look for HTML escapes
	    if (mPos == 2) {
	      if (msgBuf[1] == '#') {
		// skip over HTML character escape, like &#x2018;
		mPos = 0;
		rssStat = 13;
	      }
	    } else if (mPos == 4) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("&gt;")) == 0) {
		updRss('>', 4, 6, rssValid);

	      } else if (strcasecmp_P((char *)msgBuf, PSTR("&lt;")) == 0) {
		updRss('<', 4, 6, rssValid);
	      }
	    } else if (mPos == 5) {
	      if (strcasecmp_P((char *)msgBuf, PSTR("&amp;")) == 0) {
		updRss('&', 5, 6, rssValid);
	      } else {
		updRss('&', 1, 6, rssValid);
		while((mPos != 0) && (msgBuf[0] != '<')) {
		  updRss(msgBuf[0], 1, 6, rssValid);
		}
		if (mPos != 0) rssStat = 7;
	      }
	    }
	    break;

	  case 13:
	    // skip over HTML character escape, like &#x2018;
	    // saw '&#', so skip until ';'
	    if (c == ';') {
	      // saw ending ';', start saving RSS data again
	      updRss('_', 1, 6, rssValid);
	      mPos = 0;
	    } else if (c == ' ') {
	      // saw ' ', poorly formatted HTML escape
	      updRss('_', 1, 6, rssValid);
	      mPos = 0;
	    } else if (c == '<') {
	      // saw open angle, must be an HTML tag
	      updRss('_', 0, 7, rssValid);
	    } else {
	      mPos = 0;
	    }
	    break;
	}
	if (mPos == 0) {
	  extBuf[0] = 0;
	} else {
	  memcpy((char *)extBuf, (char *)msgBuf, mPos);
	  extBuf[mPos] = 0;
	}
      } else {			// receiving web request packet
	/*
	 * Note -- EEPROM write cycles are very long (~3ms), so writing
	 * to EEPROM while receiving the packet will cause buffer over-runs.
	 * We have to buffer the EEPROM writes in RAM, then write to
	 * EEPROM after the data has been processed.
	 */
	if (webCfgMode && (cfgPos != 0)) {	// in the middle of collecting config data
	  mPos = 0;
	  if ((c == '&')
	  ||  (c == ' ')
	  ||  (c == '\r')
	  ||  (c == '\n')
	  ||  (c == 0)
	  ) {
	    rssBuf[cfgPos] = 0;		// null terminate
	    cfgPos = 0;
	    webState = 0;
	  } else if (c == '%') {	// HTML url encoding - %20 for space
	    webState = 64;
	  } else if (webState == 64) {
	    uint8_t uc;
	    uc = c - '0';
	    if (uc > 9) {
	      uc = c - ('A' - 10);
	      if (uc > 15) {
		uc = c - ('a' - 10);
		uc &= 0x7;
	      }
	    }
	    webState = 128 | uc;
	  } else if (webState >= 128) {
	    uint8_t uc, lc;
	    uc = webState & 0xf;
	    webState = 0;
	    lc = c - '0';
	    if (lc > 9) {
	      lc = c - ('A' - 10);
	      if (lc > 15) {
		lc = c - ('a' - 10);
		lc &= 0xf;
	      }
	    }
	    c = (uc << 4) | lc;
	  }
	  if ((webState == 0) && (cfgPos != 0)) {
	    rssBuf[cfgPos++] = c;
	  }
	} else {
	  mPos = 0;
	  // I had smaller code that used strstr to check for strings,
	  // but it was too slow.  This (complex) state table is much faster.
	  switch(webState) {
	    case 0:
	      if      (c == ' ') webState = 1;	// looking for ' HTTP/'
	      else if (c == 's') webState = 6;	// looking for 'ssid=' or 'sntp='
	      else if (c == 'p') webState = 10;	// looking for 'pwd='
	      else if (c == 'r') webState = 16;	// looking for 'rss='
	      else if (c == 'a') webState = 19; // looking for 'act=1'
	      else if (c == 'P') webState = 23; // looking for 'POST /'
	      cfgPos = 0;
	      break;
	    case 1:
	      if (c == 'H') webState = 2;	// looking for ' HTTP/'
	      else webState = 0;
	      break;
	    case 2:
	      if (c == 'T') webState = 3;	// looking for ' HTTP/'
	      else webState = 0;
	      break;
	    case 3:
	      if (c == 'T') webState = 4;	// looking for ' HTTP/'
	      else webState = 0;
	      break;
	    case 4:
	      if (c == 'P') webState = 5;	// looking for ' HTTP/'
	      else webState = 0;
	      break;
	    case 5:
	      if (c == '/') webCmdDone = true;	// looking for ' HTTP/'
	      webState = 0;
	      break;
	    case 6:
	      if      (c == 's') webState = 7;	// looking for 'ssid='
	      else if (c == 'n') webState = 13;	// looking for 'sntp='
	      else webState = 0;
	      break;
	    case 7:
	      if (c == 'i') webState = 8;	// looking for 'ssid='
	      else webState = 0;
	      break;
	    case 8:
	      if (c == 'd') webState = 9;	// looking for 'ssid='
	      else webState = 0;
	      break;
	    case 9:
	      if (webCfgMode && c == '=') cfgPos = RSSbuf_SSID; // looking for 'ssid='
	      webState = 0;
	      break;
	    case 10:
	      if (c == 'w') webState = 11;	// looking for 'pwd='
	      else webState = 0;
	      break;
	    case 11:
	      if (c == 'd') webState = 12;	// looking for 'pwd='
	      else webState = 0;
	      break;
	    case 12:
	      if (webCfgMode && c == '=') cfgPos = RSSbuf_PWD; // looking for 'pwd='
	      webState = 0;
	      break;
	    case 13:
	      if (c == 't') webState = 14;	// looking for 'sntp='
	      else webState = 0;
	      break;
	    case 14:
	      if (c == 'p') webState = 15;	// looking for 'sntp='
	      else webState = 0;
	      break;
	    case 15:
	      if (webCfgMode && c == '=') cfgPos = RSSbuf_SNTP; // looking for 'sntp='
	      webState = 0;
	      break;
	    case 16:
	      if (c == 's') webState = 17;	// looking for 'rss='
	      else webState = 0;
	      break;
	    case 17:
	      if (c == 's') webState = 18;	// looking for 'rss='
	      else webState = 0;
	      break;
	    case 18:
	      if (webCfgMode && c == '=') cfgPos = RSSbuf_RSS; // looking for 'rss='
	      webState = 0;
	      break;
	    case 19:
	      if (c == 'c') webState = 20;	// looking for 'act=1'
	      else webState = 0;
	      break;
	    case 20:
	      if (c == 't') webState = 21;	// looking for 'act=1'
	      else webState = 0;
	      break;
	    case 21:
	      if (c == '=') webState = 22;	// looking for 'act=1'
	      else webState = 0;
	      break;
	    case 22:
	      if (webCfgMode && c == '1') {	// looking for 'act=1'
		cfgDone = true;
		cfgPos = 0;
	      }
	      webState = 0;
	      break;
	     case 23:
	      if (c == 'O') webState = 24;	// looking for 'POST /'
	      else webState = 0;
	      break;
	     case 24:
	      if (c == 'S') webState = 25;	// looking for 'POST /'
	      else webState = 0;
	      break;
	     case 25:
	      if (c == 'T') webState = 26;	// looking for 'POST /'
	      else webState = 0;
	      break;
	     case 26:
	      if (c == ' ') webState = 27;	// looking for 'POST /'
	      else webState = 0;
	      break;
	     case 27:
	      if (c == '/') {			// looking for 'POST /'
		rssBuf[RSSbuf_SSID] = 0;
		rssBuf[RSSbuf_PWD] = 0;
		rssBuf[RSSbuf_SNTP] = 0;
		rssBuf[RSSbuf_RSS] = 0;
		rssValid = false;
		rPos = 0;
		webCfgMode = true;
		cfgPos = 0;
	      }
	      webState = 0;
	      break;
	    default:
	      webState = 0;
	      break;
	  }
	}
	if (!dataIn) {
	  if (cfgDone || webCmdDone) {
	    state = ESP8266_CIPSEND_SERVER;
	    if (webCfgMode) {			// write data to eeprom
	      writeCfgToEEprom();
	      webCfgMode = false;
	    }
	  }
	  mPos = 0;
	}
      }
    } else {
      // reading command results, not data packet
      if (c != '\r' && c != '\n') {
	// building command results
	if (mPos < (sizeof(msgBuf) - 2)) {
	  msgBuf[mPos++] = (c == 0) ? '_' : c;
	  msgBuf[mPos] = 0;
	}
#ifdef DEBUG_CMD
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
	Serial.write(c);
#else
	if (rPos < (sizeof(rssBuf) - 2)) {
	  rssBuf[rPos++] = (c == 0) ? '_' : c;
	  rssBuf[rPos] = 0;
	}
#endif
#endif
	if ((c == ':') && (strncasecmp_P((char *)msgBuf, PSTR("+IPD,"), 5)==0)) { 
	  // start receiving data packet
	  // form is +IPD,id,len:data
	  if ((mPos > 8) && (msgBuf[6] == ',')) {
	    dataIn = true;
	    timeInputDone = ms + 2000;
	    webCmdDone = false;
	    // id of 3 is RSS, id of 4 is SNTP, anything else is config web server
	    pktCid = msgBuf[5];
	    if (pktCid == '3') {
	      if (rssStat == 1) {
		rssStat = 2;
		rPos = 0;
		rssBuf[0] = 0;
	      }
	    } else if (pktCid == '4') {
	      if (sntpStat == 2) sntpStat = 3;
	    }
	    msgBuf[mPos-1] = 0;
	    dataLen = atoi((char *)&msgBuf[7]);
	    memset(msgBuf, 0, sizeof(msgBuf));
	    if (extBuf[0] != 0) {
	      strcpy((char *)msgBuf, (char *)extBuf);
	      mPos = strlen((char *)msgBuf);
	    } else {
	      mPos = 0;
	    }
	  }
	}
      } else {
#ifdef DEBUG_CMD
#if defined (__AVR_ATmega1284P__) || defined (__AVR_ATmega644P__)
	Serial.write(c);
#endif
#endif
	// command results have completed -- at end-of-line
	mPos = 0;
	if (wantReady) {
	  if (strcasestr_P((char *)msgBuf, PSTR("eady")) != 0) {
	    state = nextState;
	    timerActive = false;
	    wantReady = false;
	  }
	} else {
	  if ((strncasecmp_P((char *)msgBuf, PSTR("OK"), 2) == 0)
	  ||  (strncasecmp_P((char *)msgBuf, PSTR("SEND OK"), 7) == 0)
	  ||  (strncasecmp_P((char *)msgBuf, PSTR("no change"), 9) == 0)
	  ||  (strncasecmp_P((char *)msgBuf, PSTR("ALREAY CONNECT"), 14) == 0)
	  ||  (strncasecmp_P((char *)msgBuf, PSTR("link is not"), 11) == 0)
	  ) {
	    if (!sawBusy) {
	      state = nextState;
	      timerActive = false;
	    } else sawBusy = false;
	  } else if ((strncasecmp_P((char *)msgBuf, PSTR("ERROR"), 5) == 0)
		 ||  (strncasecmp_P((char *)msgBuf, PSTR("Link typ ERROR"), 14) == 0)
		 ||  (strncasecmp_P((char *)msgBuf, PSTR("MUX=0"), 5) == 0)
	  ) {
	    if ((state == ESP8266_UART)
	    ||  (state == ESP8266_CIPCLOSE_SERVER)
	    ||  (state == ESP8266_CIPSERVER_STOP)
            ||  (state == ESP8266_CIPCLOSE_SNTP)
            ||  (state == ESP8266_CIPCLOSE_RSS)
	      ) {
	      state = nextState;
	      timerActive = false;
	    } else if ((state == ESP8266_CIPSEND_RSS)
		   ||  (state == ESP8266_CIPSTART_RSS)
		   ||  (state == ESP8266_CIPSTART_SNTP)
		   ||  (state == ESP8266_CWJAP)
	    ) {
	      if (++errCnt < 10) {
		state = ESP8266_CWJAP;	// send failed (lost AP?), try reconnect
	      } else {
		if (state == ESP8266_CWJAP)
		  rstCnt = 3;
		state = ESP8266_RST;	// too many failures, try a reset
		rstNext = ESP8266_CWJAP;
		busyCnt = 0;
	      }
	    } else if (state != ESP8266_CIPSERVER_START) {
	      state = ESP8266_NOOP;
	    }
	  } else if (strncasecmp_P((char *)msgBuf, PSTR("busy"), 4) == 0) {
	    sawBusy = true;
	    if (++busyCnt > 60) {
	      busyCnt = 0;
	      if ((state == ESP8266_CWJAP) || (state == ESP8266_RST)) {
		state = ESP8266_RST;	// too many busies, try a reset
		rstNext = ESP8266_CWJAP;
	      } else {
		state = ESP8266_CWJAP;	// too many busies, try reconnect
	        rstCnt = 3;
	      }
	    }
	  } else if (state == ESP8266_CIFSR) {
	    /* expecting an IP address -- search for it */
	    uint8_t *ip;
	    cp = msgBuf;
	    /* skip over non-numeric characters */
	    while (*cp && ((*cp < '0') || (*cp > '9')))
	      cp++;
	    ip = cp;
	    /* skip to first dot (###.) */
	    while((*cp >= '0') && (*cp <= '9'))
	      cp++;
	    if (*cp++ == '.') {
	      /* skip to second dot (###.###.) */
	      while((*cp >= '0') && (*cp <= '9'))
		cp++;
	      if (*cp++ == '.') {
	      /* skip to third dot (###.###.###.) */
		while((*cp >= '0') && (*cp <= '9'))
		  cp++;
		if (*cp++ == '.') {
		  if ((*cp >= '0') && (*cp <= '9')) {
		    /* found an IP address, look for its end */
		    while((*cp >= '0') && (*cp <= '9'))
		      cp++;
		    *cp = 0;
		    sntpValid = false;
		    /* look for 0.0.0.0 IP, means not associated */
		    if (strcmp_P((char *)ip, PSTR("0.0.0.0")) == 0) {
		      rssValid = false;
		      rPos = 0;
		      isJoined = false;
		      doRss = false;
		      doSntp = false;
		    } else {
		      if (inApMode) {
			strcpy((char *)rssBuf, (char *)ip);
			rPos = strlen((char *)rssBuf);
			rssBuf[rPos] = ' ';
			rssBuf[rPos+1] = 0;
			rssValid = true;
		      } else {
			isJoined = true;
			c = eeprom_read_byte((uint8_t *)EEPROM_RSS);
			if ((c > 0) && (c < 127)) {
			  doRss = true;
			  timeNextRss = ms + 120000;
			}
			c = eeprom_read_byte((uint8_t *)EEPROM_SNTP);
			if ((c > 0) && (c < 127)) {
			  doSntp = true;
			  timeNextSntp = ms + 300000;
			}
			strcpy((char *)rssBuf, (char *)ip);
			rPos = strlen((char *)rssBuf);
			rssBuf[rPos] = ' ';
			rssBuf[rPos+1] = 0;
			rssValid = true;
			dispOnce = true;
		      }
		    }
		  }
		}
	      }
	    }
	  } else if (strncasecmp_P((char *)msgBuf, PSTR("STATUS:"), 7) == 0) {
	    cipStat = msgBuf[7];
	    if (cipStat == '4') {
	      state = ESP8266_CIPCLOSE_SNTP;
	      timerActive = false;
	    }
	  } else if (strncasecmp_P((char *)msgBuf, PSTR("+CIPSTATUS:"), 11) == 0) {
	    if (msgBuf[11] == '3') {
	      if (cipStat == '3') {
		state = ESP8266_CIPSEND_RSS;
		timerActive = false;
	      } else if (--rssRetry >= 0) {
		timerActive = true;
		timer = ms + 500;
	      }
	    }
	  }
	}
	msgBuf[0] = 0;
      }
    }
  }
  if (!timerActive) {
    switch(state) {
      case ESP8266_AT:
	Esp_Port.print(F("AT\r\n"));
	nextState = ESP8266_UART;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_UART:
	if ((bindx != 0) && (sentBaud <= 2)) {
	  switch(sentBaud) {
	    case 0:
	      Esp_Port.print(F("AT+UART_DEF=38400,8,1,0,0\r\n"));
	      break;
	    case 1:
	      Esp_Port.print(F("AT+UART=38400,8,1,0,0\r\n"));
	      break;
	    case 2:
	      Esp_Port.print(F("AT+CIOBAUD=38400\r\n"));
	      break;
	  }
	  sentBaud++;
	  nextState = ESP8266_AT;
	  timer = ms + 500;
	  timerActive = true;
	} else {
	  state = ESP8266_CWMODE1;
	}
	break;

      case ESP8266_CWMODE1:
	Esp_Port.print(F("AT+CWMODE=1\r\n"));
	inApMode = false;
	nextState = ESP8266_RST;
	rstNext = ESP8266_CWJAP;
	rstCnt = 3;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_RST:
	if (--rstCnt <= 0) {
	  state = nextState;
	  wantReady = false;
	} else {
	  Esp_Port.print(F("AT+RST\r\n"));
	  wantReady = true;
	  nextState = ESP8266_CIPMUX;
	  timer = ms + 5000;
	  timerActive = true;
	}
	errCnt = 0;
	break;

      case ESP8266_CIPMUX:
	Esp_Port.print(F("AT+CIPMUX=1\r\n"));
	nextState = rstNext;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CWJAP:
	Esp_Port.print(F("AT+CWJAP=\""));
	cp = (uint8_t *)EEPROM_SSID;
	while((cp < (uint8_t *)EEPROM_PWD)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c < 127)) {
	  Esp_Port.write(c);
	}
	Esp_Port.print(F("\",\""));
	cp = (uint8_t *)EEPROM_PWD;
	while((cp < (uint8_t *)EEPROM_SNTP)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c < 127)) {
	  Esp_Port.write(c);
	}
	Esp_Port.print(F("\"\r\n"));
	nextState = ESP8266_CIFSR;
	timer = ms + 10000;
	timerActive = true;
	break;

      case ESP8266_CIFSR:
	Esp_Port.print(F("AT+CIFSR\r\n"));
#ifdef DEBUG
	nextState = ESP8266_NOOP;
#else
	nextState = ESP8266_ATE0;
#endif
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_ATE0:		// disable command echo
	Esp_Port.print(F("ATE0\r\n"));
	nextState = ESP8266_NOOP;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSTART_RSS:
        Esp_Port.print(F("AT+CIPSTART=3,\"TCP\",\""));
	cp = (uint8_t *)EEPROM_RSS + 7;	// skip leading 'http://'
	while((cp < (uint8_t *)EEPROM_RSS+128)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c != '/')
	&&    (c < 127)) {
	  Esp_Port.write(c);
	}
	Esp_Port.print(F("\",80\r\n"));
	timeNextRss = ms + (random(586, 1172) * 1024); /* ten to twenty minutes */
	cipStat = 0;
	rssStat = 0;
	rssValid = false;
	rssRetry = 60;
	nextState = ESP8266_CIPSTATUS_RSS;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSTATUS_RSS:
        Esp_Port.print(F("AT+CIPSTATUS\r\n"));
	if (--rssRetry >= 0) {
	  nextState = ESP8266_CIPSTATUS_RSS;
	} else {
	  nextState = ESP8266_NOOP;
	}
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSEND_RSS:
        rPos = 4;
        strcpy_P((char *)rssBuf, PSTR("GET "));
	cp = (uint8_t *)EEPROM_RSS+7;
	while((cp < (uint8_t *)EEPROM_RSS+128)
	&&    ((c = eeprom_read_byte(cp)) != 0)
	&&    (c != '/')
	&&    (c < 127)) {
	  cp++;
	}
	while((cp < (uint8_t *)EEPROM_RSS+128)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c < 127)) {
	  rssBuf[rPos++] = c;
	}
	strcpy_P((char *)(&rssBuf[rPos]), PSTR(" HTTP/1.1\r\nHost: "));
	rPos += 17;
	cp = (uint8_t *)EEPROM_RSS+7;
	while((cp < (uint8_t *)EEPROM_RSS+128)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c != '/')
	&&    (c < 127)) {
	  rssBuf[rPos++] = c;
	}
	strcpy_P((char *)(&rssBuf[rPos]), PSTR("\r\nUser-Agent: WifiChron/1.1\r\nAccept: */*\r\nConnection: close\r\n\r\n"));
	rPos += 63;
	Esp_Port.print(F("AT+CIPSEND=3,"));
	Esp_Port.print(rPos);
	Esp_Port.print(F("\r\n"));
	delay(5);
	Esp_Port.write(rssBuf, rPos);
	rPos = 0;
	rssBuf[0] = 0;
	rssStat = 1;
	rssValid = false;
	extBuf[0] = 0;
	nextState = ESP8266_NOOP;
	timer = ms + 500;
	timerActive = true;
	timeCloseRss = ms + 10000;	// close this connection in 10 seconds
	break;

      case ESP8266_CIPCLOSE_RSS:
	if (--rssRetry >= 0) {
	  Esp_Port.print(F("AT+CIPCLOSE=3\r\n"));
	  rssStat = 0;
	  nextState = ESP8266_NOOP;
	  timer = ms + 500;
	  timerActive = true;
	} else {
	  state = ESP8266_NOOP;
	  timerActive = false;
	}
	break;

      case ESP8266_CIPSTART_SNTP:
        Esp_Port.print(F("AT+CIPSTART=4,\"UDP\",\""));
	cp = (uint8_t *)EEPROM_SNTP;
	while((cp < (uint8_t *)EEPROM_RSS)
	&&    ((c = eeprom_read_byte(cp++)) != 0)
	&&    (c < 127)) {
	  Esp_Port.write(c);
	}
	Esp_Port.print(F("\",123\r\n"));
	sntpStat = 1;
	timeNextSntp = ms + (random(1024, 2048) * 2048); // 35 - 69 minutes later
	timeCloseSntp = ms + 5000;	// close this connection in 5 seconds
	nextState = ESP8266_CIPSEND_SNTP;
	rssRetry = 10;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSEND_SNTP:
        memset(msgBuf, 0, 48);
	extBuf[0] = 0;
	mPos = 0;
	msgBuf[0] = 0x1b;
	Esp_Port.print(F("AT+CIPSEND=4,48\r\n"));
	delay(5);
	Esp_Port.write(msgBuf, 48);
	sntpStat = 2;
	nextState = ESP8266_NOOP;
	if (--rssRetry < 0) {
	  state = ESP8266_NOOP;
	}
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPCLOSE_SNTP:
	if (--rssRetry >= 0) {
	  Esp_Port.print(F("AT+CIPCLOSE=4\r\n"));
	  sntpStat = 0;
	  nextState = ESP8266_NOOP;
	  timer = ms + 500;
	  timerActive = true;
	} else {
	  state = ESP8266_NOOP;
	  timerActive = false;
	}
	break;

      case ESP8266_CWMODE2:
	Esp_Port.print(F("AT+CWMODE=2\r\n"));
	inApMode = true;
	nextState = ESP8266_CWSAP;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CWSAP:
	Esp_Port.print(F("AT+CWSAP=\"WiFiChron\",\"WiFiChron\",3,3\r\n"));
	nextState = ESP8266_RST;
	rstNext = ESP8266_CIPSERVER_START;
	rstCnt = 3;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSERVER_START:
	Esp_Port.print(F("AT+CIPSERVER=1,80\r\n"));
	cfgDone = false;
	webCfgMode = false;
	webState = 0;
	cfgPos = 0;
	nextState = ESP8266_CIFSR;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSEND_SERVER:
	sendCfgWebPage();
	webCfgMode = false;
	webState = 0;
	cfgPos = 0;
	nextState = ESP8266_CIPCLOSE_SERVER;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPCLOSE_SERVER:
	Esp_Port.print(F("AT+CIPCLOSE="));
	Esp_Port.write(pktCid);
	Esp_Port.print(F("\r\n"));
	if (cfgDone) {
	  nextState = ESP8266_CIPSERVER_STOP;
	} else {
	  nextState = ESP8266_NOOP;
	}
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CIPSERVER_STOP:
	Esp_Port.print(F("AT+CIPSERVER=0,80\r\n"));
	nextState = ESP8266_RST;
	rstNext = ESP8266_CWMODE1;
	rstCnt = 3;
	timer = ms + 500;
	timerActive = true;
	break;

      case ESP8266_CWQAP:
	Esp_Port.print(F("AT+CWQAP\r\n"));
	nextState = ESP8266_NOOP;
	timer = ms + 500;
	timerActive = true;
	break;

      default:
	state = ESP8266_NOOP;
	nextState = ESP8266_NOOP;
	break;
    }
  } else {
    if ((long)(ms - timer) >= 0) {
      timerActive = false;
      wantReady = false;
      if (state == ESP8266_AT) {
        if (++bindx > 3) bindx = 0;
	Esp_Port.begin(bauds[bindx]);
	if (rPos < (sizeof(rssBuf) - 3)) {
	  rssBuf[rPos++] = bindx + '0';
	  rssBuf[rPos++] = ' ';
	  rssBuf[rPos] = 0;
	}
      }
    }
  }
  if (isJoined && (state == ESP8266_NOOP)) {
    if (doRss && ((long)(ms - timeNextRss)) >= 0) {
      state = ESP8266_CIPSTART_RSS;
      busyCnt = 0;
    } else if (doSntp && ((long)(ms - timeNextSntp) >= 0)) {
      state = ESP8266_CIPSTART_SNTP;
      busyCnt = 0;
    }
  }
  if (dataIn && ((long)(ms - timeInputDone) >= 0)) {
    dataIn = false;
    if (!(			// not receiving
	(sntpStat == 3)		// SNTP packet
    ||  (rssStat >= 2)		// RSS packet
    )) {
      // must be web server request packet
      if (cfgDone || webCmdDone) {
	state = ESP8266_CIPSEND_SERVER;
	if (webCfgMode) {			// write data to eeprom
	  writeCfgToEEprom();
	  webCfgMode = false;
	  cfgPos = 0;
	}
      }
    }
  }
  if ((rssStat != 0)
  &&  (state == ESP8266_NOOP)
  &&  ((long)(ms - timeCloseRss) >= 0)) {
    state = ESP8266_CIPCLOSE_RSS;
    rssRetry = 5;
  }
  if ((sntpStat != 0)
  &&  (state == ESP8266_NOOP)
  &&  ((long)(ms - timeCloseSntp) >= 0)) {
    state = ESP8266_CIPCLOSE_SNTP;
    rssRetry = 5;
  }
  if (sntpSynced && ((long)(ms - timeNotSntpSynced) >= 0)) {
    sntpSynced = false;
  }
}

void CEsp8266::getMsg(char *buf, int8_t sz)
{
  char c;
  int8_t i;

  if (rPos <= 0) {
    buf[0] = 0;
    return;
  }
  orPos = 0;
  i = 0;
  do {
    c = rssBuf[orPos++];
    buf[i++] = c;
  } while((c != 0) && (i < sz) && (orPos < sizeof(rssBuf)));
}

void CEsp8266::nextMsgPart(char *buf, int8_t sz)
{
  int8_t i;
  char c;

  if (rPos <= 0) {
    buf[0] = 0;
    return;
  }
  i = 0;
  do {
    c = rssBuf[orPos++];
    buf[i++] = c;
  } while((c != 0) && (i < sz) & (orPos < sizeof(rssBuf)));
}

CEsp8266 esp8266;
#endif	// WANT_ESP8266
