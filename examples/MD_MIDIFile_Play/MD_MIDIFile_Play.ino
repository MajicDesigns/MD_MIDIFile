// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms
//  3 LEDs (optional) - to display current status and beat. 
//  Change pin definitions for specific hardware setup - defined below.

#include <SdFat.h>
#include <MD_MIDIFile.h>

#define USE_MIDI  1   // set to 1 to enable MIDI output, otherwise debug output

#if USE_MIDI // set up for direct MIDI serial output

#define DEBUG(x)
#define DEBUGX(x)
#define DEBUGS(s)
#define SERIAL_RATE 31250

#else // don't use MIDI to allow printing debug statements

#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define DEBUGS(s) Serial.print(F(s))
#define SERIAL_RATE 57600

#endif // USE_MIDI


// SD chip select pin for SPI comms.
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
const uint8_t SD_SELECT = 10;

// LED definitions for status and user indicators
const uint8_t READY_LED = 7;      // when finished
const uint8_t SMF_ERROR_LED = 6;  // SMF error
const uint8_t SD_ERROR_LED = 5;   // SD error
const uint8_t BEAT_LED = 4;       // toggles to the 'beat'

const uint16_t WAIT_DELAY = 2000; // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

// The files in the tune list should be located on the SD card 
// or an error will occur opening the file and the next in the 
// list will be opened (skips errors).
const char *tuneList[] = 
{
  "LOOPDEMO.MID",  // simplest and shortest file
  "BANDIT.MID",
  "ELISE.MID",
  "TWINKLE.MID",
  "GANGNAM.MID",
  "FUGUEGM.MID",
  "POPCORN.MID",
  "AIR.MID",
  "PRDANCER.MID",
  "MINUET.MID",
  "FIRERAIN.MID",
  "MOZART.MID",
  "FERNANDO.MID",
  "SONATAC.MID",
  "SKYFALL.MID",
  "XMAS.MID",
  "GBROWN.MID",
  "PROWLER.MID",
  "IPANEMA.MID",
  "JZBUMBLE.MID",
};

// These don't play as they need more than 16 tracks but will run if MIDIFile.h is changed
//#define MIDI_FILE  "SYMPH9.MID"     // 29 tracks
//#define MIDI_FILE  "CHATCHOO.MID"   // 17 tracks
//#define MIDI_FILE  "STRIPPER.MID"   // 25 tracks

SdFat	SD;
MD_MIDIFile SMF;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial.write(pev->data[0] | pev->channel);
    Serial.write(&pev->data[1], pev->size-1);
  }
  else
    Serial.write(pev->data, pev->size);
#endif
  DEBUG("\n");
  DEBUG(millis());
  DEBUG("\tM T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
  DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed through the midi communications interface. Most sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nS T");
  DEBUG(pev->track);
  DEBUG(": Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void setup(void)
{
  // Set up LED pins
  pinMode(READY_LED, OUTPUT);
  pinMode(SD_ERROR_LED, OUTPUT);
  pinMode(SMF_ERROR_LED, OUTPUT);
  pinMode(BEAT_LED, OUTPUT);

  // reset LEDs
  digitalWrite(READY_LED, LOW);
  digitalWrite(SD_ERROR_LED, LOW);
  digitalWrite(SMF_ERROR_LED, LOW);
  digitalWrite(BEAT_LED, LOW);
  
  Serial.begin(SERIAL_RATE);

  DEBUG("\n[MidiFile Play List]");

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    DEBUG("\nSD init fail!");
    digitalWrite(SD_ERROR_LED, HIGH);
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);

  digitalWrite(READY_LED, HIGH);
}

void tickMetronome(void)
// flash a LED to the beat
{
  static uint32_t lastBeatTime = 0;
  static boolean  inBeat = false;
  uint16_t  beatTime;

  beatTime = 60000/SMF.getTempo();    // msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
  if (!inBeat)
  {
    if ((millis() - lastBeatTime) >= beatTime)
    {
      lastBeatTime = millis();
      digitalWrite(BEAT_LED, HIGH);
      inBeat = true;
    }
  }
  else
  {
    if ((millis() - lastBeatTime) >= 100)	// keep the flash on for 100ms only
    {
      digitalWrite(BEAT_LED, LOW);
      inBeat = false;
    }
  }
}

void loop(void)
{
  static enum { S_IDLE, S_PLAYING, S_END, S_WAIT_BETWEEN } state = S_IDLE;
  static uint16_t currTune = ARRAY_SIZE(tuneList);
  static uint32_t timeStart;

  switch (state)
  {
  case S_IDLE:    // now idle, set up the next tune
    {
      int err;

      DEBUGS("\nS_IDLE");

      digitalWrite(READY_LED, LOW);
      digitalWrite(SMF_ERROR_LED, LOW);

      currTune++;
      if (currTune >= ARRAY_SIZE(tuneList))
        currTune = 0;

      // use the next file name and play it
      DEBUG("\nFile: ");
      DEBUG(tuneList[currTune]);
      err = SMF.load(tuneList[currTune]);
      if (err != MD_MIDIFile::E_OK)
      {
        DEBUG(" - SMF load Error ");
        DEBUG(err);
        digitalWrite(SMF_ERROR_LED, HIGH);
        timeStart = millis();
        state = S_WAIT_BETWEEN;
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
    if (!SMF.isEOF())
    {
      if (SMF.getNextEvent())
        tickMetronome();
    }
    else
      state = S_END;
    break;

  case S_END:   // done with this one
    DEBUGS("\nS_END");
    SMF.close();
    midiSilence();
    timeStart = millis();
    state = S_WAIT_BETWEEN;
    DEBUGS("\nWAIT_BETWEEN");
    break;

  case S_WAIT_BETWEEN:    // signal finished with a dignified pause
    digitalWrite(READY_LED, HIGH);
    if (millis() - timeStart >= WAIT_DELAY)
      state = S_IDLE;
    break;

  default:
    state = S_IDLE;
    break;
  }
}