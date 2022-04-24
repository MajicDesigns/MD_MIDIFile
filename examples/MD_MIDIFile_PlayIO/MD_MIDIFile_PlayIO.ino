// Test playing a files from the SD card triggering I/O for each note played.
// Example program to demonstrate the the MIDFile library playing through a 
// user instrument
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms
//  I/O defined for custom notes output - Change pin definitions for specific 
//      hardware or other hardware actuation - defined below.

#include <SdFat.h>
#include <MD_MIDIFile.h>

#define USE_DEBUG  0   // set to 1 to enable MIDI output, otherwise debug output

#if USE_DEBUG // set up for direct DEBUG output

#define DEBUGS(s)    do { Serial.print(F(s)); } while (false)
#define DEBUG(s, x)  do { Serial.print(F(s)); Serial.print(x); } while(false)
#define DEBUGX(s, x) do { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(x, HEX); } while(false)

#else

#define DEBUGS(s)
#define DEBUG(s, x)
#define DEBUGX(s, x)

#endif // USE_DEBUG

// SD chip select pin for SPI comms.
// Default SD chip select is the SPI SS pin (10 on Uno, 53 on Mega).
const uint8_t SD_SELECT = SS;

const uint16_t WAIT_DELAY = 2000; // ms

// Define constants for MIDI channel voice message IDs
const uint8_t NOTE_OFF = 0x80;    // note on
const uint8_t NOTE_ON = 0x90;     // note off. NOTE_ON with velocity 0 is same as NOTE_OFF
const uint8_t POLY_KEY = 0xa0;    // polyphonic key press
const uint8_t CTL_CHANGE = 0xb0;  // control change
const uint8_t PROG_CHANGE = 0xc0; // program change
const uint8_t CHAN_PRESS = 0xd0;  // channel pressure
const uint8_t PITCH_BEND = 0xe0;  // pitch bend

// Define constants for MIDI channel control special channel numbers
const uint8_t CH_RESET_ALL = 0x79;    // reset all controllers
const uint8_t CH_LOCAL_CTL = 0x7a;    // local control
const uint8_t CH_ALL_NOTE_OFF = 0x7b; // all notes off
const uint8_t CH_OMNI_OFF = 0x7c;     // omni mode off
const uint8_t CH_OMNI_ON = 0x7d;      // omni mode on 
const uint8_t CH_MONO_ON = 0x7e;      // mono mode on (Poly off)
const uint8_t CH_POLY_ON = 0x7f;      // poly mode on (Omni off)

// The files in should be located on the SD card
const char fileName[] = "LOOPDEMO.MID";
//const char fileName[] = "TWINKLE.MID";
//const char fileName[] = "POPCORN.MID";

const uint8_t ACTIVE = HIGH;
const uint8_t SILENT = LOW;

SDFAT	SD;
MD_MIDIFile SMF;

// Define the list of I/O pins used by the application to play notes.
// Put all the I/O PIN numbers here!
const uint8_t PROGMEM pinIO[] = { 3, 4, 5, 6, 7, 8, 9, A0, A1, A2, A3 };

// Define an index into the I/O pin array each MIDI note.
// Put the array index of the pin in the pinIO array here (eg, 2 if the  
// note is played using the third pin defined in the array). This is used 
// by playNote() to work out which pin to drive ACTIVE or SILENT.
const uint8_t PROGMEM noteIO[128] =
{
  // C  C#   D  D#   E   F   G  G#   A   B  B#       Notes 
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //   0- 11: Octave  0
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  12- 23: Octave  1
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  24- 35: Octave  2
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  36- 47: Octave  3
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  48- 59: Octave  4
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  60- 71: Octave  5
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  72- 83: Octave  6
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  84- 95: Octave  7
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //  96-107: Octave  8
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, // 108-119: Octave  9
     0,  1,  2,  3,  4,  5,  6                  // 120-127: Octave 10
};

void playNote(uint8_t note, bool state)
{
  if (note > 128) return;

  uint8_t idx = pgm_read_byte(noteIO + note);
  uint8_t pin = pgm_read_byte(pinIO + idx);

  digitalWrite(pin, state);
}

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a MIDI event needs to be processed.
// This callback is set up in the setup() function.
// Note: MIDI Channel 10 (pev->channel == 9) is for percussion instruments
{
  DEBUG("\n", millis());
  DEBUG("\tM T", pev->track);
  DEBUG(":  Ch ", pev->channel+1);
  DEBUGS(" Data");
  for (uint8_t i=0; i<pev->size; i++)
    DEBUGX(" ", pev->data[i]);

  // Handle the event through our I/O interface
  switch (pev->data[0])
  {
  case NOTE_OFF:    // [1]=note no, [2]=velocity
    DEBUGS(" NOTE_OFF");
    playNote(pev->data[1], SILENT);
    break;

  case NOTE_ON:     // [1]=note_no, [2]=velocity
    DEBUGS(" NOTE_ON");
    // Note ON with velocity 0 is the same as off
    playNote(pev->data[1], (pev->data[2] == 0) ? SILENT : ACTIVE);
    break;

  case POLY_KEY:    // [1]=key no, [2]=pressure
    DEBUGS(" POLY_KEY");
    break;

  case PROG_CHANGE: // [1]=program no
    DEBUGS(" PROG_CHANGE");
    break;

  case CHAN_PRESS:  // [1]=pressure value
    DEBUGS(" CHAN_PRESS");
    break;

  case PITCH_BEND:  // [1]=MSB, [2]=LSB
    DEBUGS(" PITCH_BLEND");
    break;

  case CTL_CHANGE:  // [1]=controller no, [2]=controller value
  {
    DEBUGS(" CTL_CHANGE");
    switch (pev->data[1])
    {
    default:              // non reserved controller
      break;

    case CH_RESET_ALL:    // no data
      DEBUGS(" CH_RESET_ALL");
      break;

    case CH_LOCAL_CTL:    // data[2]=0 off, data[1]=127 on
      DEBUGS(" CH_LOCAL_CTL");
      break;

    case CH_ALL_NOTE_OFF: // no data
      DEBUGS(" CH_ALL_NOTE_OFF");
      for (uint8_t i = 0; i < ARRAY_SIZE(pinIO); i++)
      {
        uint8_t pin = pgm_read_byte(pinIO + i);
        digitalWrite(pin, SILENT);
      }
      break;

    case CH_OMNI_OFF:     // no data
      DEBUGS(" CH_OMNI_OFF");
      break;

    case CH_OMNI_ON:      // no data
      DEBUGS(" CH_OMNI_ON");
      break;

    case CH_MONO_ON:      // data[2]=0 for all, otherwise actual qty
      DEBUGS(" CH_MONO_ON");
      break;

    case CH_POLY_ON:      // no data
      DEBUGS(" CH_POLY_ON");
      break;
    }
  }
  break;
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so at the 
// end of a playing a song, explicitely turn off all the notes and sound 
// on every channel.
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = CTL_CHANGE;
  ev.data[ev.size++] = CH_ALL_NOTE_OFF;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void setup(void)
{
#if USE_DEBUG
  Serial.begin(57600);
#endif
  DEBUGS("\n[MidiFile Play I/O]");

  // initialise I/O
  for (uint8_t i = 0; i < ARRAY_SIZE(pinIO); i++)
  {
    uint8_t pin = pgm_read_byte(pinIO + i);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    DEBUGS("\nSD init fail!");
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
}

void loop(void)
{
  static enum { S_IDLE, S_PLAYING, S_END, S_PAUSE } state = S_IDLE;
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:    // now idle, set up the next tune
    {
      int err;

      DEBUGS("\nS_IDLE");

      // play the file name
      DEBUG("\nFile: ", fileName);
      err = SMF.load(fileName);
      if (err != MD_MIDIFile::E_OK)
      {
        DEBUG(" - SMF load Error ", err);
        timeStart = millis();
        state = S_PAUSE;
        DEBUGS("\nWAIT_BETWEEN");
      }
      else
      {
        DEBUGS("\nS_PLAYING");
        state = S_PLAYING;
      }
    }
    break;

  case S_PLAYING: // play the file
    DEBUGS("\nS_PLAYING");
    SMF.getNextEvent();
    if (SMF.isEOF())
      state = S_END;
    break;

  case S_END:   // done with this one
    DEBUGS("\nS_END");
    SMF.close();
    midiSilence();
    timeStart = millis();
    state = S_PAUSE;
    DEBUGS("\nPAUSE");
    break;

  case S_PAUSE:    // signal finished with a dignified pause
    if (millis() - timeStart >= WAIT_DELAY)
      state = S_IDLE;
    break;

  default:
    state = S_IDLE;
    break;
  }
}