//
// Software options
//

// used to select what is in the XBee slot
#define WANT_ESP8266		/* ESP8266 wireless Ethernet */
//#define WANT_GPS		/* GPS with NMEA output on serial */
//#define WANT_BLUETOOTH	/* Bluetooth module */

// optional additional features
#define WANT_PROVERBS		/* proverb list */
#define PROVERBS_32K		/* size of proverb list, ~32KB worth */
//#define PROVERBS_16K		/* size of proverb list, ~16KB worth */
//#define PROVERBS_8K		/* size of proverb list,  ~8KB worth */
//#define PROVERBS_4K		/* size of proverb list,  ~4KB worth */
//#define PROVERBS_2K		/* size of proverb list,  ~2KB worth */
#define WANT_MOONPHASE		/* display phase of moon message */

#define NUM_DISPLAYS	1	/* number of displays */


// end of user configuration options

// make sure only one of ESP8266, GPS, or Bluetooth is selected
#ifdef WANT_ESP8266
 #undef WANT_GPS
 #undef WANT_BLUETOOTH
#endif
#ifdef WANT_GPS
 #undef WANT_BLUETOOTH
#endif
