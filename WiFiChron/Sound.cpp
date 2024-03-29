// Sound.cpp
 
//*********************************************************************************************************
//*	Edit History, started April, 2011
//*	please put your initials and comments here anytime you make changes
//*********************************************************************************************************
//* Apr  12/11 (rp) Added "Are You Sleeping" song
//* May  29/11 (rp) Added chime();
//* Apr  24/12 (mcm) moved alarm song to PROGMEM;
//* May  05/12 (mcm) use int8_t or int16_t instead of int where appropriate
//* May  28/12 (mcm) restructured to eliminate calls to delay()
//* Aug  24/12 (mcm) add 2ms delay between tone() calls
//* Sep  06/12 (mcm) Added silenceAlarm();
//* Aug  14/13 (mcm) restructured to save space -- tone is now octave+index into
//*		     frequency table instead of frequency itself.
//* Sep  19/13 (mcm) fix bug where chime stops working after alarm sounds
//* Feb  21/15 (mcm) Modified for WiFiChron
//*********************************************************************************************************


#if ARDUINO < 100
  #include <WProgram.h>
#else
  #include <Arduino.h>
#endif
#include "Sound.h"


#define _USE_ARE_YOU_SLEEPING  // Change this line into comment if you want the "old' 5 tone sound


struct Note {
	uint8_t	tone;		// note number
	uint8_t	duration;	// length of time for note, in 16ms increments
};

int8_t noteIndex;
unsigned long noteTime;
int8_t extraNotes;
boolean notesUp;
boolean alarmActive;
uint8_t speakerPin;


//*********************************************************************************************************
void setupSpeaker(uint8_t spkr)
{
  pinMode(spkr, OUTPUT);
  digitalWrite(spkr, LOW);
  speakerPin = spkr;

  extraNotes = 0;
  noteIndex = -1;
  noteTime = 0;
  alarmActive = false;
}


#include <avr/pgmspace.h>

const uint16_t Freqs[] PROGMEM =
{
  FREQ_C11,  FREQ_CS11, FREQ_D11,  FREQ_DS11, FREQ_E11,  FREQ_F11,
  FREQ_FS11, FREQ_G11,  FREQ_GS11, FREQ_A11,  FREQ_AS11, FREQ_B11
};

const struct Note Notes[] PROGMEM =
{
  { NOTE_F7,	10	},
  { NOTE_FS7,	14	},
  { 0,		0	},
#define ALARM_INDEX	3

#ifdef _USE_ARE_YOU_SLEEPING
// Are You Sleeping - Frere Jacques
  { NOTE_D5,	28	},
  { NOTE_E5,	28	},
  { NOTE_FS5,	28	},
  { NOTE_D5,	28	},
  { NOTE_D5,	28	},
  { NOTE_E5,	28	},
  { NOTE_FS5,	28	},
  { NOTE_D5,	28	},
  { NOTE_FS5,	28	},
  { NOTE_G5,	28	},
  { NOTE_A5 ,	56	},
  { NOTE_FS5,	28	},
  { NOTE_G5,	28	},
  { NOTE_A5,	56	},
  { NOTE_A5,	18	},
  { NOTE_B5,	10	},
  { NOTE_A5,	14	},
  { NOTE_G5,	14	},
  { NOTE_FS5,	28	},
  { NOTE_D5,	28	},
  { NOTE_A5,	18	},
  { NOTE_B5,	10	},
  { NOTE_A5,	14	},
  { NOTE_G5,	14	},
  { NOTE_FS5,	28	},
  { NOTE_D5,	28	},
  { NOTE_D5,	28	},
  { NOTE_A4,	28	},
  { NOTE_D5,	56	},
  { NOTE_D5,	28	},
  { NOTE_A4,	28	},
  { NOTE_D5,	56	},
#else
  { NOTE_D4,	32	},
  { NOTE_E4,	32	},
  { NOTE_C4,	32	},
  { NOTE_C3,	32	},
  { NOTE_G3,	64	},
#endif
  { 0,		0	}		// indicate last note
};

//*********************************************************************************************************
void soundAlarm()
{
  // alarm can be stopped by pressing any of the buttons;

  if (!alarmActive) {
    noteIndex = ALARM_INDEX;
    noteTime = millis();
    extraNotes = 0;
    alarmActive = true;
  }
}

//*********************************************************************************************************
// Stop the alarm from sounding (if active)
//
void silenceAlarm()
{
  if (alarmActive) {
    alarmActive = false;
    if (noteIndex > 0) {
      noTone(speakerPin);
      noteIndex = -1;
    }
  }
  digitalWrite(speakerPin, LOW);
}


void checkSound(unsigned long ms, boolean isSounding)
{
  uint8_t atone, adur;
	
  if (noteIndex < 0) {
    digitalWrite(speakerPin, LOW);
    return;
  }

  if ((long)(ms - noteTime) < 0)
    return;

  atone = pgm_read_byte_near(&(Notes[noteIndex].tone));
  adur  = pgm_read_byte_near(&(Notes[noteIndex].duration));
  if (adur != 0) {
    uint16_t sdur;

    sdur = adur << 4;
    soundNote(atone, sdur);
    noteTime = ms + sdur + 2;
    noteIndex++;
    return;
  }
  if (!isSounding) {
    noteIndex = -1;
    alarmActive = false;
    digitalWrite(speakerPin, LOW);
    return;
  }
  if (extraNotes == 0) {
    noteTime = ms + 1000;
    extraNotes = NOTE_C4;
    notesUp = true;
    return;
  }
  noteTime = ms + 152;
  soundNote(extraNotes, 150);
  if (notesUp) {
    extraNotes++;
    if ((extraNotes & 0xf) >= 12) {
      extraNotes += 4;
    }
    if (extraNotes >= NOTE_G7) {
      notesUp = false;
    }
  } else {
    extraNotes--;
    if ((extraNotes & 0xf) >= 12) {
      extraNotes -= 4;
    }
    if (extraNotes <= NOTE_C4) {
      notesUp = true;
    }
  }
}

//*********************************************************************************************************
void soundChimeShort()
{
  if (!alarmActive) {
    noteIndex = 1;
    noteTime = millis();
  }
}

//*********************************************************************************************************
void soundChimeLong()
{

  if (!alarmActive) {
    noteIndex = 0;
    noteTime = millis();
  }
}

//*********************************************************************************************************
void soundNote(uint8_t note, uint16_t dur)
{
  uint16_t freq;
  uint8_t octave;

  octave = note >> 4; 
  note = note & 0xf;
  freq = pgm_read_word_near(&Freqs[note]);
  freq >>= (11 - octave);

  tone(speakerPin, freq, dur);
}
