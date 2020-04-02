// Utility to dump a MIDI file to the Serial monitor without too much interpretation.
// Useful for debugging a file but not generally used.
// DOES NOT require the MIDIFile library to work.

#include <SdFat.h>

// REMEMBER TO TURN ON LINE ENDING IN SERIAL MONITOR TO NEWLINE
/*
 * SD chip select pin.
 * Arduino Ethernet shield, pin 4.
 * Default SD chip select is the SPI SS pin (10).
 */
const uint8_t SD_SELECT = 10;

// states for the state machine
enum fsm_state { STATE_BEGIN, STATE_PROMPT, STATE_READ_FNAME, STATE_OPEN, STATE_DUMP, STATE_CLOSE };

// MIDI file section headers
const uint8_t HDR_SIZE = 4;
const uint8_t HDR_ERR =  0;
const uint8_t HDR_MTHD = 1;
const uint8_t HDR_MTRK = 2;

// File handler
SdFat sd;
SdFile myFile;

int ReadHeaderType(SdFile *f)
{
  char c[HDR_SIZE+1];

  f->fgets(c, HDR_SIZE+1);
  c[HDR_SIZE] = '\0';
  
  if (strcmp(c, "MTrk") == 0)
    return(HDR_MTRK);
  else if (strcmp(c, "MThd") == 0)
    return(HDR_MTHD);
  else
    return(HDR_ERR);  
}

void dumpBuffer(SdFile *f, int len)
{
  const uint8_t CHARPERLINE = 8;
  uint8_t  data[CHARPERLINE];
  int      tcount = 0, lcount;
    
  for (int j=0; j<len; j+=CHARPERLINE)
  {
    if (j!=0) Serial.println();

    // load the data    
    for (lcount=0; (lcount<CHARPERLINE) && (tcount++ < len); lcount++)
    {
      data[lcount] = f->read();
    }

    // hex dump  
  {
    int i;

    for (i=0; i<lcount; i++)
    {
      Serial.write(' ');
      if (data[i]<=0xf) 
        Serial.write('0');
      Serial.print(data[i], HEX);
    }
    for (; i<CHARPERLINE; i++)
    {
      Serial.print("   ");
    }
  }
    Serial.print("\t");
    
    // ASCII dump
    for (int i=0; (i<lcount); i++)
    {
      if ((data[i] >= ' ') && (data[i] <= '~'))
        Serial.print((char)data[i]);
      else
        Serial.print('.');
      Serial.print(" ");
    }
  }
}

void ProcessMTHD(SdFile *f)
{
  Serial.print("Length:\t\t");
  Serial.println(readMultiByte(f, 4));
  Serial.print("File Version:\t");
  Serial.println(readMultiByte(f, 2));
  Serial.print("Tracks:\t\t");
  Serial.println(readMultiByte(f, 2));
  Serial.print("Speed:\t\t");
  Serial.println(readMultiByte(f, 2));
} 

void ProcessMTRK(SdFile *f)
{
  uint32_t  l = readMultiByte(f, 4);
  
  Serial.print("Length:\t\t");
  Serial.println(l);
  
  dumpBuffer(f, l);
} 

uint32_t readMultiByte(SdFile *f, uint8_t n)
{
  uint32_t  value = 0L;
  
  for (uint8_t i=0; i<n; i++)
  {
    value = (value << 8) + f->read();
  }
  
  return(value);
}

uint32_t readVarLen(SdFile *f)
{
  uint32_t  value;
  char      c;
  
  if ((value = f->read()) & 0x80)
  {
    value &= 0x7f;
    do
    {
      c = f->read();
      value = (value << 7) + (c & 0x7f);
    } while (c & 0x80);
  }
  
  return (value);
}


void setup(void)
{
  Serial.begin(57600);
  Serial.println("[MIDI File Raw Dump]");
  
  if (!sd.begin(SD_SELECT, SPI_HALF_SPEED)) 
    sd.initErrorHalt();
}

void loop(void)
{
  static fsm_state state = STATE_BEGIN;
  static char	fname[20];

  switch (state)
  {
  case STATE_BEGIN:
    Serial.println("\nFile list from SD card:\n");
    sd.ls(Serial);
    state = STATE_PROMPT;
    break;

  case STATE_PROMPT:
    Serial.print("\nEnter file name: ");
    state = STATE_READ_FNAME;
    break;

  case STATE_READ_FNAME:
    {
      uint8_t	len = 0;
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

      Serial.println(fname);
      state = STATE_OPEN;
    }
    break;

  case STATE_OPEN:
    // open the file for reading:
    if (!myFile.open(fname, O_READ)) 
    {
      Serial.println("Opening file read failed");
      state = STATE_PROMPT;
    }
    state = STATE_DUMP;
    break;

  case STATE_DUMP:
    while (myFile.peek() != -1)
    {
      switch (ReadHeaderType(&myFile))
      {
        case HDR_MTHD:
          Serial.println("MThd ->");
          ProcessMTHD(&myFile); 
          Serial.println();
          break;
      
        case HDR_MTRK: 
          Serial.println("MTrk ->");
          ProcessMTRK(&myFile); 
          Serial.println();
          break;
      
        case HDR_ERR:  
          Serial.println("Not a valid header file"); 
          myFile.seekEnd();
          break;
      }  
    }
    state = STATE_CLOSE;
    break;

  case STATE_CLOSE:
    myFile.close();
    state = STATE_BEGIN;
    break;

  default:
    state = STATE_PROMPT;	// reset in case things go awry
  }
}
