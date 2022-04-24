// Look at the internals of a MIDI file, as interpreted by the library.
// Good for close level debugging of a file and how and in what order it
// is it is being parsed by the library.
// Also good just for curiosity of what is in the file!

#include <SdFat.h>
#include <MD_MIDIFile.h>

// MIDIFile.h library settings must be set to values below before compiling
// #define  DUMP_DATA         1
// #define  SHOW_UNUSED_META  1
//
// -= REMEMBER =-
//  * Turn line ending in Serial Monitor to NEWLINE
//  * Turn DUMP_DATA and SHOW_METADATA after back to default (0) when done
//

// SD chip select pin.
// Default SD chip select is the SPI SS pin (10 on Uno, 53 on Mega).
const uint8_t SD_SELECT = SS;

// states for the state machine
enum fsm_state { STATE_BEGIN, STATE_PROMPT, STATE_READ_FNAME, STATE_LOAD, STATE_PROCESS, STATE_CLOSE };

SDFAT SD;
MD_MIDIFile SMF;

void setup(void)
{
  Serial.begin(57600);
  Serial.println(F("[MIDI_File_Dumper]"));

  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.println(F("SD init failed!"));
    while (true) ;
  }

  SMF.begin(&SD);
}

void loop(void)
{
  int  err;
  static fsm_state state = STATE_BEGIN;
  static char fname[50];

  switch (state)
  {
  case STATE_BEGIN:
  case STATE_PROMPT:
    Serial.print(F("\n\n"));
    SD.ls(Serial);
    Serial.print(F("\nEnter file name: "));
    state = STATE_READ_FNAME;
    break;

  case STATE_READ_FNAME:
    {
      uint8_t len = 0;
      char c; 

      // read until end of line
      do
      {
        while (!Serial.available())
          ;  // wait for the next character
        c = Serial.read();
        fname[len++] = c;
      } while (c != '\n');

      // properly terminate
      --len;
      fname[len++] = '\0';

      Serial.print(fname);
      state = STATE_LOAD;
    }
    break;

  case STATE_LOAD:
    err = SMF.load(fname);
    if (err != MD_MIDIFile::E_OK)
    {
      Serial.print(F("\nSMF load Error "));
      Serial.print(err);
      state = STATE_PROMPT;
    }
    else
    {
      SMF.dump();
      state = STATE_PROCESS;
    }
    break;

  case STATE_PROCESS:
    if (!SMF.isEOF())
      SMF.getNextEvent();
    else
      state = STATE_CLOSE;
    break;

  case STATE_CLOSE:
    SMF.close();
    state = STATE_BEGIN;
    break;

  default:
    state = STATE_BEGIN;	// reset in case things go awry
  }
}


