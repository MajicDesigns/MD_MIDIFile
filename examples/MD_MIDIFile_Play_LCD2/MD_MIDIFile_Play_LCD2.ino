// User selects a file from the SD card list on the LCD display and plays 
// the music selected.
// Example program to demonstrate the use of the MIDIFile library.
// Variation on the original example contributed by R Foschini.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms.
//  LCD interface - assumed to be 2 rows 16 chars. Change LCD 
//    pin definitions for hardware setup. Uses the MD_UISwitch library 
//    (found at https://github.com/MajicDesigns/MD_UISwitch) to read and manage 
//    the LCD display buttons.
//
// MC 2020-04-02 Updated to current usage but untested. LCD library throws up warnings
//               probably due toincoirrect version ofthe lib installed.
//

#include <avr/wdt.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_UISwitch.h>
#include <LiquidCrystal_I2C.h>

#define DEBUG_ON 1

#if DEBUG_ON

#define DEBUG(x)  Serial.print(x)
#define DEBUGX(x) Serial.print(x, HEX)
#define SERIAL_RATE 57600

#else

#define DEBUG(x)
#define DEBUGX(x)
#define SERIAL_RATE 31250

#endif

// SD Hardware defines ---------
// SPI select pin for SD card (SPI comms).
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10) on Uno, 53 on Mega.
// Other hardware will be different as documented for that hardware.
const uint8_t SD_SELECT = 53;

const uint8_t PUSHBUTTON1 = 2;
const uint8_t PUSHBUTTON2 = 3;
const uint8_t BEAT_LED1 = 4;
const uint8_t BEAT_LED2 = 5;

// LCD display defines ---------
const uint8_t LCD_ROWS = 4;
const uint8_t LCD_COLS = 16;

// LCD user defined characters
char PAUSE = '\1';
uint8_t cPause[8] = { 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x00 };

// LCD pin switches ---------
const uint8_t LCD_KEYS = A0;
MD_UISwitch_Analog::uiAnalogKeys_t kt[] =
{
  {  30, 30, 'R' },  // Right
  { 100, 40, 'U' },  // Up
  { 250, 50, 'D' },  // Down
  { 410, 60, 'L' },  // Left
  { 640, 60, 'S' },  // Select
};

// Library objects -------------
LiquidCrystal_I2C LCD(0x27, LCD_COLS, LCD_ROWS);
SdFat SD;
MD_MIDIFile SMF;
MD_UISwitch_Analog LCDKey(LCD_KEYS, kt, ARRAY_SIZE(kt));

// Playlist handling -----------
const uint8_t FNAME_SIZE = 13;               // file names 8.3 to fit onto LCD display
const char* PLAYLIST_FILE = "PLAYLIST.TXT";  // file of file names
const char* MIDI_EXT = ".MID";               // MIDI file extension
uint16_t  plCount = 0;
char fname[FNAME_SIZE + 1];
static uint16_t  cycleTime;

// Enumerated types for the FSM(s)
enum lcd_state { LSBegin, LSSelect, LSShowFile };
enum midi_state { MSBegin, MSLoad, MSOpen, MSProcess, MSClose };
enum seq_state { LCDSeq, MIDISeq };

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
#if DEBUG_ON
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
// Called by the MIDIFile library when a System Exclusive (sysex) file event needs 
// to be processed thru the midi communications interface. Most sysex events cannot 
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

void metaCallback(const meta_event *pev) 
// Called by the MIDIFile library when a META file event needs 
// to be processed thru the midi communications interface. Most meta events cannot 
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  DEBUG("\nMETA T");
  DEBUG(pev->track);
  DEBUG(": Type 0x");
  DEBUGX(pev->type);
  DEBUG(" Data ");
  switch (pev->type) 
  {
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
  static uint32_t lastBeatTime = 0;
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
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120; // all sounds off
  ev.data[ev.size++] = 0;

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
  if (!plFile.open(PLAYLIST_FILE, O_CREAT | O_WRITE))
  {
    LCDErrMessage("PL create fail", true);
  }
  else
  {
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
        if (strcasecmp(MIDI_EXT, &fname[strlen(fname) - strlen(MIDI_EXT)]) == 0)
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
  }

  return(count);
}

// FINITE STATE MACHINES -----------------------------

seq_state lcdFSM(seq_state curSS)
// Handle selecting a file name from the list (user input)
{
  static lcd_state s = LSBegin;
  static uint8_t plIndex = 0;
  static SdFile plFile;  // play list file

  // LCD state machine
  switch (s)
  {
  case LSBegin:
    LCDMessage(0, 0, "Select play:", true);
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
    if (LCDKey.read() == MD_UISwitch::KEY_PRESS)
    {
      switch (LCDKey.getKey())
        // Keys are mapped as follows:
        // Select:  move on to the next state in the state machine
        // Left:    use the previous file name (move back one file name)
        // Right:   use the next file name (move forward one file name)
        // Up:      move to the first file name
        // Down:    move to the last file name
      {
      case 'S': // Select
        DEBUG("\n>Play");
        curSS = MIDISeq;    // switch mode to playing MIDI in main loop
        s = LSBegin;        // reset for next time
        break;

      case 'L': // Left
        DEBUG("\n>Previous");
        if (plIndex != 0)
          plIndex--;
        s = LSShowFile;
        break;

      case 'U': // Up
        DEBUG("\n>First");
        plIndex = 0;
        s = LSShowFile;
        break;

      case 'D': // Down
        DEBUG("\n>Last");
        plIndex = plCount - 1;
        s = LSShowFile;
        break;

      case 'R': // Right
        DEBUG("\n>Next");
        if (plIndex != plCount - 1)
          plIndex++;
        s = LSShowFile;
        break;
      }
    }
    break;

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
    LCDMessage(0, 0, "   \1", true);
    LCDMessage(1, 0, "K  >  \xdb", true);   // string of user defined characters
    s = MSLoad;
    break;

  case MSLoad:
    // Load the current file in preparation for playing
    {
      int err;

      // Attempt to load the file
      if ((err = SMF.load(fname)) == MD_MIDIFile::E_OK)
        s = MSProcess;
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
    if (value!=oldvalue1) 
    {
      if (value<oldvalue1)
        SMF.setTempoAdjust(tempoAdjust+=5);
      oldvalue1 = value;
    }

    value = digitalRead(PUSHBUTTON2);
    if (value!=oldvalue2) 
    {
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
        sprintf(sBuf, "T%3d", SMF.getTempo());
        LCDMessage(0, LCD_COLS-strlen(sBuf), sBuf, true);
        sprintf(sBuf, "S%d/%d", SMF.getTimeSignature()>>8, SMF.getTimeSignature() & 0xf);
        LCDMessage(1, LCD_COLS-strlen(sBuf), sBuf, true);
      };
    }    
    else
      s = MSClose;

    // check the keys
    if (LCDKey.read() == MD_UISwitch::KEY_PRESS)
    {
      switch (LCDKey.getKey())
      {
      case 'L': midiSilence();  SMF.restart();    break;  // Rewind
      case 'R': s = MSClose;                      break;  // Stop
      case 'U': SMF.pause(true); midiSilence();   break;  // Pause
      case 'D': SMF.pause(false);                 break;  // Start
      case 'S':                                   break;  // Nothing assigned to this key
      }
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
  
  // initialize LCD keys
  LCDKey.begin();
  LCDKey.enableDoublePress(false);
  LCDKey.enableLongPress(false);
  LCDKey.enableRepeat(false);

  // initialise LCD display
  LCD.begin(LCD_COLS, LCD_ROWS);
  LCD.backlight();
  LCD.clear();
  LCD.noCursor();
  LCDMessage(0, 0, "  Midi  Player  ", false);
  LCDMessage(1, 0, "  ------------  ", false);

  // Load characters to the LCD
  LCD.createChar(PAUSE, cPause);

  pinMode(PUSHBUTTON1, INPUT_PULLUP);
  pinMode(PUSHBUTTON2, INPUT_PULLUP);
  pinMode(BEAT_LED1, OUTPUT);
  pinMode(BEAT_LED2, OUTPUT);

  // initialize SDFat
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
    LCDErrMessage("SD init fail!", true);

  plCount = createPlaylistFile();
  if (plCount == 0)
    LCDErrMessage("No files", true);

  // initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
  SMF.setMetaHandler(metaCallback);
  midiSilence();

  delay(750);   // allow the welcome to be read on the LCD
  wdt_enable(WDTO_250MS);
}

void loop(void)
// only need to look after 2 things - the user interface (LCD_FSM) 
// and the MIDI playing (MIDI_FSM). While playing we have a different 
// mode from choosing the file, so the FSM will run alternately, depending 
// on which state we are currently in.
{
  static seq_state s = LCDSeq;
  static uint32_t lastUpdate = 0;
  static uint32_t lastMillis = 0;
  uint32_t currMillis = millis();

  cycleTime = currMillis - lastMillis;
  if (cycleTime>=25 || currMillis - lastUpdate>5000) 
  {
    lastUpdate = currMillis;

    char sBuf[5];
    sprintf(sBuf, "C%3d", cycleTime);
    LCDMessage(3, 17, sBuf, false);
  }
  lastMillis=currMillis;

  switch (s)
  {
    case LCDSeq:  s = lcdFSM(s);	break;
    case MIDISeq: s = midiFSM(s);	break;
    default: s = LCDSeq;
  }

  wdt_reset();
}

