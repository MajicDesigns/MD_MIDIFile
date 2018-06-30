// User selects a file from the SD card list on the LCD display and plays 
// the music selected.
// Example program to demonstrate the use of the MIDIFile library.
// Variation on the original example contributed by R Foschini.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms.
//  LCD interface - assumed to be 2 rows 16 chars. Change LCD 
//    pin definitions for hardware setup. Uses the MD_AButton library 
//    (found at https://github.com/MajicDesigns/MD_AButton) to read and manage 
//    the LCD display buttons.
//

#include <avr/wdt.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_AButton.h>
#include <LiquidCrystal_I2C.h>

#include "FSMtypes.h" // FSM enumerated types

#define MAIN_DEBUG_MODE 1

#if MAIN_DEBUG_MODE

#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define SERIAL_RATE 115200

#else

#define DEBUG(x)
#define DEBUGX(x)
#define SERIAL_RATE 31250

#endif

// SD Hardware defines ---------
// SPI select pin for SD card (SPI comms).
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  53

#define  PUSHBUTTON1 2
#define  PUSHBUTTON2 3
#define BEAT_LED1 4
#define BEAT_LED2 5

// LCD display defines ---------
#define  LCD_ROWS  4
#define  LCD_COLS  20

// LCD user defined characters
#define PAUSE '\1'
uint8_t cPause[8] = 
{
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b00000
};

// LCD pin definitions ---------
#define  LCD_KEYS  KEY_ADC_PORT

// Library objects -------------
LiquidCrystal_I2C LCD(0x27, LCD_COLS, LCD_ROWS);
SdFat SD;
MD_MIDIFile SMF;
MD_AButton  LCDKey(LCD_KEYS);

// Playlist handling -----------
#define FNAME_SIZE    13              // 8.3 + '\0' character file names
#define PLAYLIST_FILE "PLAYLIST.TXT"  // file of file names
#define MIDI_EXT    ".MID"            // MIDI file extension
uint16_t  plCount = 0;
static uint16_t  cycleTime;
//static bool  cycleUpdate;

// MIDI callback functions for MIDIFile library ---------------

void midiCallback(midi_event * const pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  Serial1.write(pev->data[0] | pev->channel);
  Serial1.write(&pev->data[1], pev->size-1);

  Serial2.write(pev->data[0] | pev->channel);
  Serial2.write(&pev->data[1], pev->size-1);
#else
  Serial.write(pev->data[0] | pev->channel);
  Serial.write(&pev->data[1], pev->size-1);
#endif
#if MAIN_DEBUG_MODE
  DEBUG("\nMIDI T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
#endif
}

void sysexCallback(sysex_event * const pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed thru the midi communications interface. MOst sysex events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nSYSEX T");
  DEBUG(pev->track);
  DEBUG(": Data ");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void metaCallback(const meta_event *pev) {
  DEBUG("\nMETA T");
  DEBUG(pev->track);
  DEBUG(": Type 0x");
  DEBUGX(pev->type);
  DEBUG(" Data ");
  switch (pev->type) {
  case 0x1:
  case 0x2:
  case 0x3:
  case 0x4:
  case 0x5:
  case 0x6:
  case 0x7:
    DEBUG(pev->chars);
    break;

  default:
    for (uint8_t i=0; i<pev->size; i++)
    {
      DEBUGX(pev->data[i]);
      DEBUG(' ');
    }
    break;
  }
}

void tickMetronome(void)
// flash a LED to the beat
{
  static uint32_t  lastBeatTime = 0;
  static boolean  inBeat = false;
  uint16_t  beatTime;
  static int beatLed;
  static int beatCounter = 1;

  beatTime = 60000/SMF.getTempo();    // msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
  if (!inBeat)
  {
    if ((millis() - lastBeatTime) >= beatTime)
    {
      lastBeatTime = millis();
      if (++beatCounter > (SMF.getTimeSignature() & 0xf))
        beatCounter = 1;
      beatLed = beatCounter == 1 ? BEAT_LED1 : BEAT_LED2;
      digitalWrite(beatLed, HIGH);
      
      inBeat = true;
    }
  }
  else
  {
    if ((millis() - lastBeatTime) >= 50) // keep the flash on for 50ms only
    {
      digitalWrite(beatLed, LOW);
      inBeat = false;
    }
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
  ev.size=3;
  ev.data[0] = 0xb0;
  ev.data[1] = 120; // all sounds off
  ev.data[2] = 0;
  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);

  ev.data[1] = 121; // reset controller
  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

// LCD Message Helper functions -----------------
void LCDMessage(uint8_t r, uint8_t c, const char *msg, bool clrEol = false)
// Display a message on the LCD screen with optional spaces padding the end
{
  LCD.setCursor(c, r);
  LCD.print(msg);
  if (clrEol)
  {
    c += strlen(msg);
    while (c++ < LCD_COLS)
      LCD.write(' ');
  }
}

void LCDErrMessage(const char *msg, bool fStop)
{
  LCDMessage(1, 0, msg, true);
  DEBUG("\nLCDErr: ");
  DEBUG(msg);
  while (fStop) ;   // stop here if told to
  delay(2000);      // if not stop, pause to show message
}

// Create list of files for menu --------------

uint16_t createPlaylistFile(void)
// create a play list file on the SD card with the names of the files.
// This will then be used in the menu.
{
  SdFile    plFile;   // play list file
  SdFile    mFile;    // MIDI file
  uint16_t  count = 0;// count of files
  char      fname[FNAME_SIZE];

  // open/create the play list file
  if (!plFile.open(PLAYLIST_FILE, O_CREAT|O_WRITE))
    LCDErrMessage("PL create fail", true);

  SD.vwd()->rewind();
  while (mFile.openNext(SD.vwd(), O_READ))
  {
    mFile.getName(fname, FNAME_SIZE);

    DEBUG("\nFile ");
    DEBUG(count);
    DEBUG(" ");
    DEBUG(fname);

    if (mFile.isFile())
    {
      if (strcmp(MIDI_EXT, &fname[strlen(fname)-strlen(MIDI_EXT)]) == 0)
      // only include files with MIDI extension
      {
        plFile.write(fname, FNAME_SIZE);
        count++;
      }
    }
    mFile.close();
  }
  DEBUG("\nList completed");

  // close the play list file
  plFile.close();

  return(count);
}

// FINITE STATE MACHINES -----------------------------

seq_state lcdFSM(seq_state curSS)
// Handle selecting a file name from the list (user input)
{
  static lcd_state s = LSBegin;
  static uint8_t plIndex = 0;
  static char fname[FNAME_SIZE];
  static SdFile plFile;  // play list file

  // LCD state machine
  switch (s)
  {
  case LSBegin:
    LCDMessage(0, 0, "Select music:", true);
    if (!plFile.isOpen())
    {
      if (!plFile.open(PLAYLIST_FILE, O_READ))
        LCDErrMessage("PL file no open", true);
    }
    s = LSShowFile;
    break;

  case LSShowFile:
    plFile.seekSet(FNAME_SIZE*plIndex);
    plFile.read(fname, FNAME_SIZE);

    LCDMessage(1, 0, fname, true);
    LCD.setCursor(LCD_COLS-2, 1);
    LCD.print(plIndex == 0 ? ' ' : '<');
    LCD.print(plIndex == plCount-1 ? ' ' : '>');
    s = LSSelect;
    break;

  case LSSelect:
    switch (LCDKey.getKey())
    // Keys are mapped as follows:
    // Select:  move on to the next state in the state machine
    // Left:    use the previous file name (move back one file name)
    // Right:   use the next file name (move forward one file name)
    // Up:      move to the first file name
    // Down:    move to the last file name
    {
      case 'S': // Select
        s = LSGotFile;
        break;

      case 'L': // Left
        if (plIndex != 0) 
          plIndex--;
        s = LSShowFile;
        break;

      case 'U': // Up
        plIndex = 0;
        s = LSShowFile;
        break;

      case 'D': // Down
        plIndex = plCount-1;
        s = LSShowFile;
        break;

      case 'R': // Right
        if (plIndex != plCount-1) 
          plIndex++;
        s = LSShowFile;
        break;
    }
    break;

  case LSGotFile:
    // copy the file name and switch mode to playing MIDI
    SMF.setFilename(fname);
    curSS = MIDISeq;
    // fall through to default state

  default:
    s = LSBegin;
    break;
  }  

  return(curSS);
}

seq_state midiFSM(seq_state curSS)
// Handle playing the selected MIDI file
{
  bool value;
  static int tempoAdjust = 0;
  static int oldvalue1 = HIGH, oldvalue2 = HIGH;
  static midi_state s = MSBegin;
  
  switch (s)
  {
  case MSBegin:
    // Set up the LCD 
    LCDMessage(0, 0, SMF.getFilename(), true);
    LCDMessage(1, 0, "K  \xdb  \1  >", true);   // string of user defined characters
    s = MSLoad;
    break;

  case MSLoad:
    // Load the current file in preparation for playing
    {
      int  err;

      // Attempt to load the file
      if ((err = SMF.load()) == -1)
      {
        s = MSProcess;
      }
      else
      {
        char aErr[16];

        sprintf(aErr, "SMF error %03d", err);
        LCDErrMessage(aErr, false);
        s = MSClose;
      }
    }
    break;

  case MSProcess:
    // Play the MIDI file
    value = digitalRead(PUSHBUTTON1);
    if (value!=oldvalue1) {
      if (value<oldvalue1)
        SMF.setTempoAdjust(tempoAdjust+=5);
      oldvalue1 = value;
    }
    value = digitalRead(PUSHBUTTON2);
    if (value!=oldvalue2) {
      if (value<oldvalue2)
        SMF.setTempoAdjust(tempoAdjust-=5);
      oldvalue2 = value;
    }
    if (!SMF.isEOF())
    {
      if (SMF.getNextEvent())
      {
        tickMetronome();

        char  sBuf[10];
        sprintf(sBuf, "%3d", SMF.getTempo());
        LCDMessage(0, LCD_COLS-strlen(sBuf), sBuf, true);
        sprintf(sBuf, "%d/%d", SMF.getTimeSignature()>>8, SMF.getTimeSignature() & 0xf);
        LCDMessage(1, LCD_COLS-strlen(sBuf), sBuf, true);
      };
    }    
    else
      s = MSClose;

    // check the keys
    switch (LCDKey.getKey())
    {
      case 'S': SMF.restart();    break;  // Rewind
      case 'L': s = MSClose;      break;  // Stop
      case 'U': SMF.pause(true);  break;  // Pause
      case 'D': SMF.pause(false); break;  // Start
      case 'R':                   break;  // Nothing assigned to this key
    }

    break;

  case MSClose:
    // close the file and switch mode to user input
    SMF.close();
    midiSilence();
    curSS = LCDSeq;
    // fall through to default state

  default:
    s = MSBegin;
    break;
  }

  return(curSS);
}

void setup(void)
{
  int  err;

  // initialise MIDI output stream
  Serial.begin(SERIAL_RATE);
  while (!Serial)
    ;
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  Serial1.begin(31250);
  while (!Serial1)
    ;
  Serial2.begin(31250);
  while (!Serial2)
    ;
#endif
  // initialise LCD display
  LCD.begin(LCD_COLS, LCD_ROWS);
  LCD.init();
  LCD.backlight();
  LCD.clear();
  LCD.noCursor();
  LCDMessage(0, 0, "  Midi  Player  ", false);
  LCDMessage(1, 0, "  ------------  ", false);

  // Load characters to the LCD
  LCD.createChar(PAUSE, cPause);

  pinMode(LCD_KEYS, INPUT);
  pinMode(PUSHBUTTON1, INPUT_PULLUP);
  pinMode(PUSHBUTTON2, INPUT_PULLUP);
  pinMode(BEAT_LED1, OUTPUT);
  pinMode(BEAT_LED2, OUTPUT);

  // initialise SDFat
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
    LCDErrMessage("SD init fail!", true);

  plCount = createPlaylistFile();
  if (plCount == 0)
    LCDErrMessage("No files", true);

  midiSilence();

  // initialise MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
  SMF.setMetaHandler(metaCallback);

  delay(750);   // allow the welcome to be read on the LCD
  wdt_enable(WDTO_250MS);
}

void loop(void)
// only need to look after 2 things - the user interface (LCD_FSM) 
// and the MIDI playing (MIDI_FSM). While playing we have a different 
// mode from choosing the file, so the FSM will run alternately, depending 
// on which state we are currently in.
{
  static long lastUpdate = 0;
  static long lastMillis = 0;
  long currMillis = millis();
  cycleTime = currMillis-lastMillis;
  if (cycleTime>=25 || currMillis-lastUpdate>5000) {
//    DEBUG("cycle: ");
//    DEBUG(cycleTime);
//    DEBUG('\n');
    lastUpdate = currMillis;

    char sBuf[5];
    sprintf(sBuf, "%3d", cycleTime);
    LCDMessage(3, 17, sBuf, false);
  }
  lastMillis=currMillis;

  static seq_state s = LCDSeq;

  switch (s)
  {
    case LCDSeq:  s = lcdFSM(s);	break;
    case MIDISeq: s = midiFSM(s);	break;
    default: s = LCDSeq;
  }
  wdt_reset();
}

