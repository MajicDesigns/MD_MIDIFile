// User selects a file from the SD card list on the LCD display and plays 
// the music selected.
// Example program to demonstrate the use of the MIDIFile library.
//
// Hardware required:
//	SD card interface - change SD_SELECT for SPI comms.
//	LCD interface - assumed to be 2 rows 16 chars. Change LCD 
//	  pin definitions for hardware setup. Uses the MD_AButton library 
//    (found at http://arduinocode.codeplex.com/releases) to read and manage 
//    the LCD display buttons.
//

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_AButton.h>
#include <LiquidCrystal.h>

#include "FSMtypes.h"		// FSM enumerated types

#define	DEBUG_MODE	0

#if DEBUG_MODE

#define	DEBUG(x)	Serial.print(x)
#define	DEBUGX(x)	Serial.print(x, HEX)
#define	SERIAL_RATE	57600

#else

#define	DEBUG(x)
#define	DEBUGX(x)
#define	SERIAL_RATE	31250

#endif

// SD Hardware defines ---------
// SPI select pin for SD card (SPI comms).
// Arduino Ethernet shield, pin 4.
// Default SD chip select is the SPI SS pin (10).
// Other hardware will be different as documented for that hardware.
#define  SD_SELECT  10

// LCD display defines ---------
#define  LCD_ROWS  2
#define  LCD_COLS  16

// LCD user defined characters
#define	PAUSE		'\1'
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

// Playlist handling -----------
#define	FNAME_SIZE		13				// 8.3 + '\0' character file names
#define	PLAYLIST_FILE	"PLAYLIST.TXT"	// file of file names
#define	MIDI_EXT		".MID"			// MIDI file extension
uint16_t	plCount = 0;

// MIDI callback functions for MIDIFile library ---------------

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if !DEBUG_MODE
	if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
	{
		Serial.write(pev->data[0] | pev->channel);
		Serial.write(&pev->data[1], pev->size-1);
	}
	else
		Serial.write(pev->data, pev->size);
#endif
  DEBUG("\nM T");
  DEBUG(pev->track);
  DEBUG(":  Ch ");
  DEBUG(pev->channel+1);
  DEBUG(" Data ");
  for (uint8_t i=0; i<=pev->size; i++)
  {
	DEBUGX(pev->data[i]);
    DEBUG(' ');
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs 
// to be processed thru the midi communications interface. MOst sysex events cannot 
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
	midi_event	ev;

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
	while (fStop) ;		// stop here if told to
	delay(2000);		// if not stop, pause to show message
}

// Create list of files for menu --------------

uint16_t createPlaylistFile(void)
// create a play list file on the SD card with the names of the files.
// This will then be used in the menu.
{
	SdFile		plFile;		// play list file
	SdFile		mFile;		// MIDI file
	uint16_t	count = 0;// count of files
	char		  fname[FNAME_SIZE];

	// open/create the play list file
	if (!plFile.open(PLAYLIST_FILE, O_CREAT|O_WRITE))
		LCDErrMessage("PL create fail", true);

  SD.vwd()->rewind();
  while (mFile.openNext(SD.vwd(), O_READ))
	{
    mFile.getFilename(fname);

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
	static uint8_t	plIndex = 0;
	static char	fname[FNAME_SIZE];
	static SdFile	plFile;		// play list file

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
		// Select:	move on to the next state in the state machine
		// Left:	use the previous file name (move back one file name)
		// Right:	use the next file name (move forward one file name)
		// Up:		move to the first file name
		// Down:	move to the last file name
		{
			case 'S':	// Select
				s = LSGotFile;
				break;

			case 'L':	// Left
				if (plIndex != 0) 
					plIndex--;
				s = LSShowFile;
				break;

			case 'U':	// Up
				plIndex = 0;
				s = LSShowFile;
				break;

			case 'D':	// Down
				plIndex = plCount-1;
				s = LSShowFile;
				break;

			case 'R':	// Right
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
	static midi_state s = MSBegin;
  
	switch (s)
	{
	case MSBegin:
		// Set up the LCD 
		LCDMessage(0, 0, SMF.getFilename(), true);
		LCDMessage(1, 0, "K  \xdb  \1  >", true);		// string of user defined characters
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
				char	aErr[16];

				sprintf(aErr, "SMF error %03d", err);
				LCDErrMessage(aErr, false);
				s = MSClose;
			}
		}
		break;

	case MSProcess:
		// Play the MIDI file
		if (!SMF.isEOF())
    {
			if (SMF.getNextEvent())
      {
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
			case 'S':	SMF.restart();		break;	// Rewind
			case 'L':	s = MSClose;		  break;	// Stop
			case 'U':	SMF.pause(true);	break;	// Pause
			case 'D':	SMF.pause(false);	break;	// Start
			case 'R':						        break;	// Nothing assigned to this key
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

	// initialise LCD display
	LCD.begin(LCD_COLS, LCD_ROWS);
	LCD.clear();
	LCD.noCursor();
	LCDMessage(0, 0, "  Midi  Player  ", false);
	LCDMessage(1, 0, "  ------------  ", false);

	// Load characters to the LCD
	LCD.createChar(PAUSE, cPause);

	pinMode(LCD_KEYS, INPUT);

	// initialise SDFat
	if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
		LCDErrMessage("SD init fail!", true);

	plCount = createPlaylistFile();
	if (plCount == 0)
		LCDErrMessage("No files", true);
	
	// initialise MIDIFile
	SMF.begin(&SD);
	SMF.setMidiHandler(midiCallback);
	SMF.setSysexHandler(sysexCallback);

	delay(750);		// allow the welcome to be read on the LCD
}

void loop(void)
// only need to look after 2 things - the user interface (LCD_FSM) 
// and the MIDI playing (MIDI_FSM). While playing we have a different 
// mode from choosing the file, so the FSM will run alternately, depending 
// on which state we are currently in.
{
	static seq_state	s = LCDSeq;

	switch (s)
	{
		case LCDSeq:  s = lcdFSM(s);	break;
		case MIDISeq: s = midiFSM(s);	break;
		default: s = LCDSeq;
	}
}



