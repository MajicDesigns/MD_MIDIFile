// Command Line Interface (CLI or CONSOLE) to play MIDI files from the SD card.
// 
// Enter commands on the serial monitor to control the application
// Change the CLI and MIDI outputs to suit your hardware.
// This should adjust by itself to use Serial1 if USE_SOFTWARSERIAL is set to 0.
//
// Library Dependencies
// MD_cmdProcessor located at https://github.com/MajicDesigns/MD_cmdProcessor
//

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_cmdProcessor.h>

#define USE_SOFTWARESERIAL 1

// Define Serial comms parameters
#if USE_SOFTWARESERIAL
#include <SoftwareSerial.h>

const uint8_t MIDI_OUT = 2;     // Arduino TX pin number
const uint8_t MIDI_IN = 3;      // Arduino RX pin number (never used)
SoftwareSerial MidiOut(MIDI_IN, MIDI_OUT);

#define MIDI MidiOut
#else // not USE_SOFTWARESERIAL
#define MIDI Serial1
#endif

#define CONSOLE Serial

const uint32_t CONSOLE_RATE = 57600;
const uint32_t MIDI_RATE = 31250;

// SD chip select pin for SPI comms.
// Default SD chip select is the SPI SS pin (10 on Uno, 53 on Mega).
const uint8_t SD_SELECT = SS;         

// Miscellaneous
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
void(*hwReset) (void) = 0;            // declare reset function @ address 0

// Global Data
bool printMidiStream = false;   // flag to print the real time midi stream
SdFat SD;
MD_MIDIFile SMF;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  // Send the midi data
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    MIDI.write(pev->data[0] | pev->channel);
    MIDI.write(&pev->data[1], pev->size - 1);
  }
  else
    MIDI.write(pev->data, pev->size);

  // Print the data if enabled
  if (printMidiStream)
  {
    CONSOLE.print(F("\nT"));
    CONSOLE.print(pev->track);
    CONSOLE.print(F(": Ch "));
    if (pev->channel + 1 < 10) CONSOLE.print(F("0"));
    CONSOLE.print(pev->channel + 1);
    CONSOLE.print(F(" ["));
    for (uint8_t i = 0; i < pev->size; i++)
    {
      if (i != 0) CONSOLE.print(F(" "));
      if (pev->data[i] <= 0x0f)
        CONSOLE.print(F("0"));
      CONSOLE.print(pev->data[i], HEX);
    }
    CONSOLE.print(F("]"));
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

const char *SMFErr(int err)
{
  const char *DELIM_OPEN = "[";
  const char *DELIM_CLOSE = "]";

  static char szErr[30];
  char szFmt[10];

  static const char PROGMEM E_OK[] = "\nOK";
  static const char PROGMEM E_FILE_NUL[] = "Empty filename";
  static const char PROGMEM E_OPEN[] = "Cannot open";
  static const char PROGMEM E_FORMAT[] = "Not MIDI";
  static const char PROGMEM E_HEADER[] = "Header size";
  static const char PROGMEM E_FILE_FMT[] = "File unsupported";
  static const char PROGMEM E_FILE_TRK0[] = "File fmt 0; trk > 1";
  static const char PROGMEM E_MAX_TRACK[] = "Too many tracks";
  static const char PROGMEM E_NO_CHUNK[] = "no chunk"; // n0
  static const char PROGMEM E_PAST_EOF[] = "past eof"; // n1
  static const char PROGMEM E_UNKNOWN[] = "Unknown";

  static const char PROGMEM EF_ERR[] = "\n%d %s";    // error number
  static const char PROGMEM EF_TRK[] = "Trk %d,";  // for errors >= 10

  if (err == MD_MIDIFile::E_OK)    // this is a simple message
    strcpy_P(szErr, E_OK);
  else              // this is a complicated message
  {
    strcpy_P(szFmt, EF_ERR);
    sprintf(szErr, szFmt, err, DELIM_OPEN);
    if (err < 10)
    {
      switch (err)
      {
      case MD_MIDIFile::E_NO_FILE:  strcat_P(szErr, E_FILE_NUL); break;
      case MD_MIDIFile::E_NO_OPEN:  strcat_P(szErr, E_OPEN); break;
      case MD_MIDIFile::E_NOT_MIDI: strcat_P(szErr, E_FORMAT); break;
      case MD_MIDIFile::E_HEADER:   strcat_P(szErr, E_HEADER); break;
      case MD_MIDIFile::E_FORMAT:   strcat_P(szErr, E_FILE_FMT); break;
      case MD_MIDIFile::E_FORMAT0:  strcat_P(szErr, E_FILE_TRK0); break;
      case MD_MIDIFile::E_TRACKS:   strcat_P(szErr, E_MAX_TRACK); break;
      default: strcat_P(szErr, E_UNKNOWN); break;
      }
    }
    else      // error encoded with a track number
    {
      char szTemp[10];

      // fill in the track number
      strcpy_P(szFmt, EF_TRK);
      sprintf(szTemp, szFmt, err / 10);
      strcat(szErr, szTemp);

      // now do the message
      switch (err % 10)
      {
      case MD_MIDIFile::E_CHUNK_ID:  strcat_P(szErr, E_NO_CHUNK); break;
      case MD_MIDIFile::E_CHUNK_EOF: strcat_P(szErr, E_PAST_EOF); break;
      default: strcat_P(szTemp, E_UNKNOWN); break;
      }
    }
    strcat(szErr, DELIM_CLOSE);
  }

  return(szErr);
}

// handler functions
void handlerHelp(char* param); // function prototype

void handlerZS(char* param) { hwReset(); }

void handlerZM(char* param) 
{ 
  CONSOLE.print(F("\nMIDI silence"));
  midiSilence(); 
  CONSOLE.print(SMFErr(MD_MIDIFile::E_OK)); 
}

void handlerCL(char* param) 
{ 
  SMF.looping(*param != '0'); 
  CONSOLE.print(F("\nLooping "));
  CONSOLE.print(SMF.isLooping()); 
  CONSOLE.print(F(" ")); 
  CONSOLE.print(SMFErr(MD_MIDIFile::E_OK)); 
}

void handlerCP(char* param) 
{ 
  SMF.pause(*param != '0'); 
  CONSOLE.print(F("\nPause "));
  CONSOLE.print(SMF.isPaused());
  CONSOLE.print(F(" "));
  CONSOLE.print(SMFErr(MD_MIDIFile::E_OK)); 
}

void handlerCD(char* param)
{
  printMidiStream = !printMidiStream;
  CONSOLE.print(SMFErr(MD_MIDIFile::E_OK));
}

void handlerCC(char* param)
{ 
  SMF.close(); 
  CONSOLE.print(F("\nMIDI close"));
  CONSOLE.print(SMFErr(MD_MIDIFile::E_OK));
}

void handlerP(char *param)
// play specified file
{
  int err;

  // clean up current environment
  SMF.close(); // close old MIDI file
  midiSilence(); // silence hanging notes

  CONSOLE.print(F("\nFile: "));
  CONSOLE.print(param);
  err = SMF.load(param); // load the new file
  CONSOLE.print(SMFErr(err));
  CONSOLE.print(F(": "));
  CONSOLE.print(SMF.getFilename());
}

void handlerF(char *param)
// set the current folder
{
  CONSOLE.print(F("\nFolder: "));
  CONSOLE.print(param);
  SMF.setFileFolder(param); // set folder
}

void handlerL(char *param)
// list the files in the current folder
{
  SdFile file;    // iterated file

  SD.vwd()->rewind();
  while (file.openNext(SD.vwd(), O_READ))
  {
    if (file.isFile())
    {
      char buf[20];

      file.getName(buf, ARRAY_SIZE(buf));
      CONSOLE.print(F("\n"));
      CONSOLE.print(buf);
    }
    file.close();
  }
  CONSOLE.print(F("\n"));
}

const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "?",  handlerHelp, "",     "Help" },
  { "h",  handlerHelp, "",     "Help" },
  { "f",  handlerF,    "fldr", "Set current folder to fldr" },
  { "l",  handlerL,    "",     "List files in current folder" },
  { "p",  handlerP,    "file", "Play the named file" },
  { "zs", handlerZS,   "",     "Software reset" },
  { "zm", handlerZM,   "",     "Silence MIDI" },
  { "cd", handlerCD,   "",     "Toggle dumping of MIDI stream" },
  { "cl", handlerCL,   "n",    "Looping (n 0=off, 1=on)" },
  { "cp", handlerCP,   "n",    "Pause (n 0=off, 1=on)" },
  { "cc", handlerCC,   "",     "Close" },
};

MD_cmdProcessor CP(CONSOLE, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param)
{
  CONSOLE.print(F("\nHelp\n===="));
  CP.help();
  CONSOLE.print(F("\n"));
}

void setup(void) // This is run once at power on
{
  MIDI.begin(MIDI_RATE); // For MIDI Output
  CONSOLE.begin(CONSOLE_RATE); // For Console I/O

  CONSOLE.print(F("\n[MidiFile CLI Player]"));
  CONSOLE.print(F("\nEnsure that console line termination is set to line feed only.\n"));

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    CONSOLE.print(F("\nSD init fail!"));
    while (true);
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  midiSilence(); // Silence any hanging notes

  // show the available commands
  handlerHelp(nullptr);
}

void loop(void)
{
  if (!SMF.isEOF()) 
    SMF.getNextEvent(); // Play MIDI data

  CP.run();  // process the CLI
}