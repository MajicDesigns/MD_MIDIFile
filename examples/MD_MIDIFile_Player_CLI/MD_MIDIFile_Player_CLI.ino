// Command Line Interface (CLI or CONSOLE) to play MIDI files from the SD card.
// 
// Enter commands on the serial monitor to control the application
// Change the CLI and MIDI outputs to suit your hardware.
// This should adjust by itself to use Serial1 if USE_SOFTWARSERIAL is set to 0.
//

#include <SdFat.h>
#include <MD_MIDIFile.h>

#define USE_SOFTWARESERIAL 1

// Define Serial comms parameters
#if USE_SOFTWARESERIAL
#include <SoftwareSerial.h>

const uint8_t MIDI_OUT = 2;     // Arduino TX pin number
const uint8_t MIDI_IN = 3;      // Arduino RX pin number (never used)
SoftwareSerial MidiOut(MIDI_IN, MIDI_OUT);

#define MIDI MidiOut
#else // USE_SOFTWARESERIAL
#define MIDI Serial1
#endif

#define CONSOLE Serial

const uint32_t CONSOLE_RATE = 57600;
const uint32_t MIDI_RATE = 31250;

// SD chip select pin for SPI comms.
// Default SD chip select is the SPI SS pin (10 on Uno, 53 on Mega).
const uint8_t SD_SELECT = SS;         

// Miscellaneous
const uint8_t RCV_BUF_SIZE = 50;      // UI character buffer size
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
void(*hwReset) (void) = 0;            // declare reset function @ address 0

// Global Data
bool printMidiStream = false;   // flag to print the real time midi stream
char rcvBuf[RCV_BUF_SIZE];  // buffer for characters received from the console
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
    CONSOLE.print(pev->channel + 1);
    CONSOLE.print(F(" Data"));
    for (uint8_t i = 0; i < pev->size; i++)
    {
      CONSOLE.print(F(" "));
      if (pev->data[i] <= 0x0f)
        CONSOLE.print(F("0"));
      CONSOLE.print(pev->data[i], HEX);
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
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void help(void)
{
  CONSOLE.print(F("\nEnsure that console line termination is set to line feed only.\n"));
  CONSOLE.print(F("\nh,?\thelp"));
  CONSOLE.print(F("\nf fldr\tset current folder to fldr"));
  CONSOLE.print(F("\nl\tlist out files in current folder"));
  CONSOLE.print(F("\np file\tplay the named file"));

  CONSOLE.print(F("\n\n* Sketch Control"));
  CONSOLE.print(F("\nz s\tsoftware reset"));
  CONSOLE.print(F("\nz m\tmidi silence"));
  CONSOLE.print(F("\nz d\tdump the real time midi stream (toggle on/off)"));

  CONSOLE.print(F("\n\n* Play Control"));
  CONSOLE.print(F("\nc ln\tlooping (n 0=off, 1=on)"));
  CONSOLE.print(F("\nc pn\tpause (n 0=off, 1=on)"));
  CONSOLE.print(F("\nc r\trestart"));
  CONSOLE.print(F("\nc c\tclose"));

  CONSOLE.print(F("\n"));
}

const char *SMFErr(int err)
{
  const char *DELIM_OPEN = "[";
  const char *DELIM_CLOSE = "]";

  static char szErr[30];
  char szFmt[10];

  static const char PROGMEM E_OK[] = "\nOK";  // -1
  static const char PROGMEM E_FILE_NUL[] = "Empty filename"; // 0 
  static const char PROGMEM E_OPEN[] = "Cannot open"; // 1
  static const char PROGMEM E_FORMAT[] = "Not MIDI"; // 2
  static const char PROGMEM E_HEADER[] = "Header size"; // 3
  static const char PROGMEM E_FILE_FMT[] = "File unsupproted"; // 5
  static const char PROGMEM E_FILE_TRK0[] = "File fmt 0; trk > 1"; // 6
  static const char PROGMEM E_MAX_TRACK[] = "Too many tracks"; // 7
  static const char PROGMEM E_NO_CHUNK[] = "no chunk"; // n0
  static const char PROGMEM E_PAST_EOF[] = "past eof"; // n1
  static const char PROGMEM E_UNKNOWN[] = "Unknown";

  static const char PROGMEM EF_ERR[] = "\n%d %s";    // error number
  static const char PROGMEM EF_TRK[] = "Trk %d,";  // for errors >= 10

  if (err == -1)    // this is a simple message
    strcpy_P(szErr, E_OK);
  else              // this is a complicated message
  {
    strcpy_P(szFmt, EF_ERR);
    sprintf(szErr, szFmt, err, DELIM_OPEN);
    if (err < 10)
    {
      switch (err)
      {
      case 0: strcat_P(szErr, E_FILE_NUL); break;
      case 1: strcat_P(szErr, E_OPEN); break;
      case 2: strcat_P(szErr, E_FORMAT); break;
      case 3: strcat_P(szErr, E_HEADER); break;
      case 4: strcat_P(szErr, E_FILE_FMT); break;
      case 5: strcat_P(szErr, E_FILE_TRK0); break;
      case 6: strcat_P(szErr, E_MAX_TRACK); break;
      case 7: strcat_P(szErr, E_NO_CHUNK); break;
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
      case 0: strcat_P(szErr, E_NO_CHUNK); break;
      case 1: strcat_P(szErr, E_PAST_EOF); break;
      default: strcat_P(szTemp, E_UNKNOWN); break;
      }
    }
    strcat(szErr, DELIM_CLOSE);
  }

  return(szErr);
}

bool recvLine(void) 
{
  const char endMarker = '\n'; // end of the Serial input line
  static byte ndx = 0;
  char c;
  bool b = false;        // true when we have a complete line

  while (CONSOLE.available() && !b) // process all available characters before eoln
  {
    c = CONSOLE.read();
    if (c != endMarker) // is the character not the end of the string termintator?
    {
      if (!isspace(c))  // filter out all the whitespace
      {
        rcvBuf[ndx] = toupper(c); // save the character
        ndx++;
        if (ndx >= RCV_BUF_SIZE) // handle potential buffer overflow
          ndx--;
        rcvBuf[ndx] = '\0';       // alwaqys maintain a valid string
      }
    }
    else
    {
      ndx = 0;          // reset buffer to receive the next line
      b = true;         // return this flag
    }
  }
  return(b);
}

void processUI(void)
{
  if (!recvLine())
    return;

  // we have a line to process
  switch (rcvBuf[0])
  {
  case 'H':
  case '?':
    help();
    break;

  case 'Z':   // resets
    switch (rcvBuf[1])
    {
    case 'S': hwReset(); break;
    case 'M': midiSilence(); CONSOLE.print(SMFErr(-1)); break;
    case 'D': printMidiStream = !printMidiStream; CONSOLE.print(SMFErr(-1)); break;
    }
    break;

  case 'C':  // control
    switch (rcvBuf[1])
    {
    case 'L': // looping
      if (rcvBuf[2] == '0')
        SMF.looping(false);
      else
        SMF.looping(true);
      CONSOLE.print(SMFErr(-1));
      break;

    case 'P': // pause
      if (rcvBuf[2] == '0')
        SMF.pause(false);
      else
        SMF.pause(true);
      CONSOLE.print(SMFErr(-1));
      break;

    case 'R': // restart
      SMF.restart();
      CONSOLE.print(SMFErr(-1));
      break;

    case 'C': // close
      SMF.close();
      CONSOLE.print(SMFErr(-1));
      break;
    }
    break;

  case 'P': // play
    {
      int err;

      // clean up current environment
      SMF.close(); // close old MIDI file
      midiSilence(); // silence hanging notes

      CONSOLE.print(F("\nRead File: "));
      CONSOLE.print(&rcvBuf[1]);
      SMF.setFilename(&rcvBuf[1]); // set filename
      CONSOLE.print(F("\nSet file : "));
      CONSOLE.print(SMF.getFilename());
      err = SMF.load(); // load the new file
      CONSOLE.print(SMFErr(err));
    }
    break;

  case 'F': // set the current folder for MIDI files
    {
      CONSOLE.print(F("\nFolder: "));
      CONSOLE.print(&rcvBuf[1]);
      SMF.setFileFolder(&rcvBuf[1]); // set folder
    }
    break;

  case 'L': // list the files in the current folder
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
    break;
  }
}

void setup(void) // This is run once at power on
{
  MIDI.begin(MIDI_RATE); // For MIDI Output
  CONSOLE.begin(CONSOLE_RATE); // For Console I/O

  CONSOLE.print(F("\n[MidiFile CLI Player]"));

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
  help();
}

void loop(void)
{
  if (!SMF.isEOF()) 
    SMF.getNextEvent(); // Play MIDI data

  processUI();  // process the User Interface
}