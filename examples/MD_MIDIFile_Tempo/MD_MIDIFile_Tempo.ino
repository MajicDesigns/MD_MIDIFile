// Play a file from the SD card in looping mode controlliong the tempo.
// Tempo is controlled wither by just setting the Tempo in the SMF object
// or by generating the MIDI ticks in this application.
//
// Example program to demonstrate the use of the MIDFile library
//
// Hardware required:
//	SD card interface - change SD_SELECT for SPI comms
//  LCD 1602 shield - change pins for LCD control
//  Key switches on the LCD shield - connected to an analog pin
// 

#include <SdFat.h>
#include <LiquidCrystal.h>
#include <MD_Abutton.h>
#include <MD_MIDIFile.h>

// Set to 1 to generate the MIDI ticks in this application
// Set to 0 to allow library to generate clock based on Tempo
#define GENERATE_TICKS  0

// Set to 1 to use the MIDI interface (ie, not debugging to serial port)
#define	USE_MIDI	1

#if USE_MIDI // set up for direct MIDI serial output

#define DEBUGS(s)
#define	DEBUG(s, x)
#define	DEBUGX(s, x)
#define	SERIAL_RATE	31250

#else // don't use MIDI to allow printing debug statements

#define DEBUGS(s)     Serial.print(s)
#define	DEBUG(s, x)	  { Serial.print(F(s)); Serial.print(x); }
#define	DEBUGX(s, x)	{ Serial.print(F(s)); Serial.print(x, HEX); }
#define	SERIAL_RATE	57600

#endif // USE_MIDI

// SD Hardware defines ---------
// SPI select pin for SD card (SPI comms).
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  10

// LCD display defines ---------
#define  LCD_ROWS  2
#define  LCD_COLS  16

// LCD pin definitions ---------
// These need to be modified for the LCD hardware setup
#define  LCD_RS    8
#define  LCD_ENA   9
#define  LCD_D4    4
#define  LCD_D5    (LCD_D4+1)
#define  LCD_D6    (LCD_D4+2)
#define  LCD_D7    (LCD_D4+3)
#define  LCD_KEYS  KEY_ADC_PORT

// Library objects -------------
LiquidCrystal LCD(LCD_RS, LCD_ENA, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
SdFat	SD;
MD_MIDIFile SMF;
MD_AButton  LCDKey(LCD_KEYS);

#define	ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]))

// The in the tune list should be located on the SD card 
// or an error will occur opening the file.
char *loopfile = "AFROCUBA.MID";  // simple and short file
#if GENERATE_TICKS
uint8_t lclBPM = 120;
#endif

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// through the midi communications interface.
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
  DEBUG("\nM T", pev->track);
  DEBUG(":  Ch ", pev->channel+1);
  DEBUGS(" Data");
  for (uint8_t i=0; i<pev->size; i++)
  {
	  DEBUGX(" ", pev->data[i]);
  }
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

void LCDErrMessage(const char *msg)
// Display error message on the LCD and block waiting
{
  LCDMessage(1, 0, msg, true);
  DEBUG("\nLCDErr: ", msg);
  while (true);		// stop here
}

void LCDBeat(bool bOn)
// Display a beat every quarter note
{
  LCD.setCursor(15, 0);
  LCD.write(bOn ? 'O' : 'o');
}

void LCDbpm(void)
// Display the BPM - depends on the mode we are running
{
  char  sBuf[LCD_COLS];
  
#if GENERATE_TICKS
  sprintf(sBuf, "BPM:%d", lclBPM, 10);
#else
  sprintf(sBuf, "BPM:%d%s%d", 
      SMF.getTempo(), (SMF.getTempoAdjust() >= 0 ? "+" : ""), SMF.getTempoAdjust());
#endif
  LCDMessage(1, 0, sBuf, true);
  sprintf(sBuf, "%d/%d", SMF.getTimeSignature() >> 8, SMF.getTimeSignature() & 0xf);
  LCDMessage(1, LCD_COLS-strlen(sBuf), sBuf, true);
}

void CheckUI(void)
// Check the button on the UI and take action - depends on the mode we are running
{
#if GENERATE_TICKS
  switch (LCDKey.getKey())
  {
    case 'S':	lclBPM = 120;  break;	// set to default
    
    case 'R':	if (lclBPM <= 245) lclBPM += 10; break;	// Increase Tempo by 10
    case 'L':	if (lclBPM >= 11)  lclBPM -= 10; break;	// Decrease Tempo by 10

    case 'U':	if (lclBPM <= 254) lclBPM++; break;	// Increase Tempo by 1
    case 'D':	if (lclBPM >= 2)   lclBPM--; break;	// Decrease Tempo by 1
  }
#else
  switch (LCDKey.getKey())
  {
    case 'S':	SMF.setTempoAdjust(0);  break;	// set to default
    
    case 'R':	SMF.setTempoAdjust(SMF.getTempoAdjust()+10); break;	// Increase Tempo by 10
    case 'L':	SMF.setTempoAdjust(SMF.getTempoAdjust()-10); break;	// Decrease Tempo by 10

    case 'U':	SMF.setTempoAdjust(SMF.getTempoAdjust()+1); break;	// Increase Tempo by 1
    case 'D':	SMF.setTempoAdjust(SMF.getTempoAdjust()-1); break;	// Decrease Tempo by 1
  }
#endif
}

#if GENERATE_TICKS
uint16_t tickClock(void)
// Check if enough time has passed for a MIDI tick and work out how many!
{
  static uint32_t lastTickCheckTime, lastTickError;
  uint8_t   ticks = 0;

  uint32_t elapsedTime = lastTickError + micros() - lastTickCheckTime;
  uint32_t tickTime = (60 * 1000000L) / (lclBPM * SMF.getTicksPerQuarterNote());	// microseconds per tick
  tickTime = (tickTime * 4) / (SMF.getTimeSignature() & 0xf); // Adjusted for time signature

  if (elapsedTime >= tickTime)
  {
    ticks = elapsedTime/tickTime;
    lastTickError = elapsedTime - (tickTime * ticks);
    lastTickCheckTime = micros();			// save for next round of checks
  }
  
  return(ticks);
}
#endif

void setup(void)
{
  int  err;

  Serial.begin(SERIAL_RATE);

  DEBUGS("\n[MidiFile Tempo]");

	// initialise LCD display
	LCD.begin(LCD_COLS, LCD_ROWS);
	LCD.clear();
	LCD.noCursor();
	LCDMessage(0, 0, loopfile, false);
	
 	pinMode(LCD_KEYS, INPUT);

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
		LCDErrMessage("SD init fail!");

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.looping(true);

  // use the next file name and play it
  DEBUG("\nFile: ", loopfile);
  SMF.setFilename(loopfile);
  err = SMF.load();
  if (err != -1)
  {
    char sBuf[20] = "SMF load err ";

    itoa(err, &sBuf[strlen(sBuf)-1], 10);
    LCDErrMessage(sBuf);
  }  
}

void loop(void)
{
  // check the user interface keys
  CheckUI();
  
	// play the file
#if GENERATE_TICKS
  static bool fBeat = false;
  static uint16_t sumTicks = 0;
  uint32_t ticks = tickClock();
  
  if (ticks > 0)
  {
    LCDBeat(fBeat);
    SMF.processEvents(ticks);  
	  SMF.isEOF();  // side effect to cause restart at EOF if looping
    LCDbpm();
    
    sumTicks += ticks;
    if (sumTicks >= SMF.getTicksPerQuarterNote())
    {
      sumTicks = 0;
      fBeat = !fBeat;
    }    
  }  
#else
	if (!SMF.isEOF()) 
  {
    if (SMF.getNextEvent())
      LCDbpm();
  }  
#endif
}