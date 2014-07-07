/*
  MD_MIDITrack.cpp - An Arduino library for processing Standard MIDI Files (SMF).
  Copyright (C) 2012 Marco Colli
  All rights reserved.

  See MIDIFile.h for complete comments

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <string.h>
#include "MD_MIDIFile.h"
#include "MD_MIDIHelper.h"

////////////////////////////////////////
// MFTrack Class code
//

void MD_MFTrack::reset(void)
{
	_length = 0;        // length of track in bytes
    _startOffset = 0;   // start of the track in bytes from start of file
	_currOffset = 0;
	_endOfTrack = false;
	_elapsedTimeTotal = 0;
	_trackId = 255;
}

MD_MFTrack::MD_MFTrack(void)
{
	reset();
}

MD_MFTrack::~MD_MFTrack()
{
}

void MD_MFTrack::close(void)
{
	reset();
}

uint32_t MD_MFTrack::getLength(void)
// size of track in bytes
{
  return _length;
}

bool MD_MFTrack::getEndOfTrack(void)
// true if end of track has been reached
{
  return _endOfTrack;
}

void MD_MFTrack::syncTime(void)
{
	_elapsedTimeTotal = 0;
}

void MD_MFTrack::restart(void)
// Start playing the track from the beginning again
{
	_currOffset = 0;
}

bool MD_MFTrack::getNextEvent(MD_MIDIFile *mf, uint32_t elapsedTime)
// track_event = <time:v> + [<midi_event> | <meta_event> | <sysex_event>]
{
	uint32_t deltaT, elapsedTicks;

	// is there anything to process?
	if (_endOfTrack)
	  return(false);

	// move the file pointer to where we left off
	mf->_fd.seekSet(_startOffset+_currOffset);

	// work out new total elapsed time
	_elapsedTimeTotal += elapsedTime;
	elapsedTicks = _elapsedTimeTotal / mf->getMicrosecondDelta();

	// Get the DeltaT from the file in order to see if enough ticks have
	// passed for the event to be active. If not, just return without
	// saving the file pointer and we will go back to the same spot next time.
	deltaT = readVarLen(&mf->_fd);
	if (elapsedTicks < deltaT)
		return(false);

	// Adjust the total elapsed time. Note use of actual DeltaT to avoid 
	// accumulation of errors as we only check for elaspedTime being >= ticks,
	// giving positive biased errors every time.
	_elapsedTimeTotal -= (deltaT * mf->getMicrosecondDelta());

	DUMPS("\ndT: ");
	DUMP(deltaT);
	DUMP("\tET: ");
	DUMP(elapsedTicks);
	DUMP('\t');

	parseEvent(mf);

	// remember the offset for next time
	_currOffset = mf->_fd.curPosition() - _startOffset;

	return(true);
}

void MD_MFTrack::parseEvent(MD_MIDIFile *mf)
// process the event from the physical file
{
	uint8_t	 eType;
	uint32_t eLen, mLen;
	sysex_event	sev;		// used for sysex callback function

	// now we have to process this event
	eType = mf->_fd.read();

	switch (eType)
	{
// ---------------------------- MIDI

	  // midi_event = any MIDI channel message, including running status
	  // Midi events (status bytes 0x8n - 0xEn) The standard Channel MIDI messages, where 'n' is the MIDI channel (0 - 15).
	  // This status byte will be followed by 1 or 2 data bytes, as is usual for the particular MIDI message. 
	  // Any valid Channel MIDI message can be included in a MIDI file.
	case 0x80 ... 0xBf:	// MIDI message with 2 parameters
	case 0xe0 ... 0xef:
		_mev.size = 3;
		_mev.data[0] = eType;
		_mev.channel = _mev.data[0] & 0xf;	// mask off the channel
		_mev.data[0] = _mev.data[0] & 0xf0;	// just the command byte
		_mev.data[1] = mf->_fd.read();
		_mev.data[2] = mf->_fd.read();
		DUMPS("[MID2] Ch: ");
		DUMP(_mev.channel);
		DUMPS(" Data: ");
		DUMPX(_mev.data[0]);
		DUMP(' ');
		DUMPX(_mev.data[1]);
		DUMP(' ');
		DUMPX(_mev.data[2]);
#if !DUMP_DATA
		if (mf->_midiHandler != NULL)
			(mf->_midiHandler)(&_mev);
#endif // !DUMP_DATA
	break;

	case 0xc0 ... 0xdf:	// MIDI message with 1 parameter
		_mev.size = 2;
		_mev.data[0] = eType;
		_mev.channel = _mev.data[0] & 0xf;	// mask off the channel
		_mev.data[0] = _mev.data[0] & 0xf0;	// just the command byte
		_mev.data[1] = mf->_fd.read();
		DUMPS("[MID1] Ch: ");
		DUMP(_mev.channel);
		DUMPS(" Data: ");
		DUMPX(_mev.data[0]);
		DUMP(' ');
		DUMPX(_mev.data[1]);

#if !DUMP_DATA
		if (mf->_midiHandler != NULL)
			(mf->_midiHandler)(&_mev);
#endif
	break;

	case 0x00 ... 0x7f:	// MIDI run on message
	{
	  // If the first (status) byte is less than 128 (0x80), this implies that MIDI 
	  // running status is in effect, and that this byte is actually the first data byte 
	  // (the status carrying over from the previous MIDI event). 
	  // This can only be the case if the immediately previous event was also a MIDI event, 
	  // ie SysEx and Meta events clear running status. This means that the _mev structure 
	  // should contain the info from the previous message in the structure's channel member 
	  // and data[0] (for the MIDI command). 
	  // Hence start saving the data at byte data[1] with the byte we have just read (eType) 
	  // and use the size member to determine how large the message is (ie, same as before).
	  _mev.data[1] = eType;
	  for (uint8_t i = 2; i < _mev.size; i++)
	  {
		_mev.data[i] = mf->_fd.read();	// next byte
	  }	

	  DUMPS("[MID+] Ch: ");
	  DUMP(_mev.channel);
	  DUMPS(" Data: ");
	  for (uint8_t i = 0; i<_mev.size; i++)
	  {
  		DUMPX(_mev.data[i]);
		DUMP(' ');
	  }

#if !DUMP_DATA
	  if (mf->_midiHandler != NULL)
		(mf->_midiHandler)(&_mev);
#endif
	}
	break;
	  
// ---------------------------- SYSEX

	case 0xf0:		// sysex_event = 0xF0 + <len:1> + <data_bytes> + 0xF7 
	case 0xf7:		// sysex_event = 0xF7 + <len:1> + <data_bytes> + 0xF7 
	{
 		uint8_t	c, i;
		sysex_event sev;

		// collect all the bytes until the 0xf7 - boundaries are included in the message
		sev.track = _trackId;
		sev.data[0] = eType;
		sev.size = mf->_fd.read();
		// The length parameter includes the 0xF7 but not the start boundary. 
		// However, it may be bigger than our buffer will allow us to store.
		#define	BUF_SIZE	(sizeof(sev.data)/sizeof(sev.data[0]))	
		sev.size = (sev.size > BUF_SIZE - 2 ? BUF_SIZE-2 : sev.size + 1);
		for (i = 1; (i < sev.size) && (c != 0xf7); i++)
		{
			c = mf->_fd.read();	// next char
			sev.data[i] = c;
		}	

		// check if we had an overflow
		if (c != 0xf7)
		{
			while ((c = mf->_fd.read()) != 0xf7)
				; // skip read all data
			sev.data[sev.size] = 0xf7;		// terminate properly - data is probably nuked anyway
		}

		DUMPS("[SYSX] Data: ");
		for (uint8_t i = 0; i<sev.size; i++)
		{
  			DUMPX(sev.data[i]);
			DUMP(' ');
		}

#if !DUMP_DATA
		if (mf->_sysexHandler != NULL)
			(mf->_sysexHandler)(&sev);
#endif
	}
	break;

// ---------------------------- META

	case 0xff:		// meta_event = 0xFF + <meta_type:1> + <length:v> + <event_data_bytes>
	  eType = mf->_fd.read();
	  mLen =  readVarLen(&mf->_fd);
	  DUMPS("[META] Type: 0x");
	  DUMPX(eType);
	  DUMPS("\tLen: ");
	  DUMP(mLen);
	  DUMP('\t');

	  switch (eType)
	  {
	  case 0x2f:	// End of track
		_endOfTrack = true;
	    DUMPS("END OF TRACK");
	  break;

	  case 0x51:	// set Tempo - really the microseconds per tick
		mf->setMicrosecondPerQuarterNote(readMultiByte(&mf->_fd, MB_TRYTE));
		DUMPS("SET TEMPO to ");
		DUMP(mf->getMicrosecondDelta());
		DUMPS(" us/tick or ");
		DUMP(mf->getTempo());
		DUMPS(" beats/min");
	  break;

	  case 0x58:	// time signature
		mf->setTimeSignature(mf->_fd.read(), (2 << mf->_fd.read()));	// denominator is 2^n
		mf->_fd.seekCur(mLen-2);
		DUMPS("SET TIME SIGNATURE to ");
		DUMP(mf->getTimeSignature()>>8);
		DUMP('/');
		DUMP(mf->getTimeSignature()&0xf);
	  break;

#if SHOW_UNUSED_META
	  case 0x0:	// Sequence Number
		DUMPS("SEQUENCE NUMBER ");
		DUMP(readMultiByte(&mf->_fd, MB_WORD));
	  break;

	  case 0x1:	// Text
		DUMPS("TEXT ");
		for (int i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x2:	// Copyright Notice
		DUMPS("COPYRIGHT ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x3:	// Sequence or Track Name
		DUMPS("SEQ/TRK NAME ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x4:	// Instrumenmt Name
		DUMPS("INSTRUMENT ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x5:	// Lyric
		DUMPS("LYRIC ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x6:	// Marker
		DUMPS("MARKER ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x7:	// Cue Point
		DUMPS("CUE POINT ");
		for (uint8_t i=0; i<mLen; i++)
			DUMP((char)mf->_fd.read());
	  break;

	  case 0x20:// Channel Prefix
		DUMPS("CHANNEL PREFIX ");
		DUMP(readMultiByte(&mf->_fd, MB_BYTE));
	  break;

	  case 0x21:// Port Prefix
		DUMPS("PORT PREFIX ");
		DUMP(readMultiByte(&mf->_fd, MB_BYTE));
	  break;

	  case 0x54:// SMPTE Offset
		DUMPS("SMPTE OFFSET ");
		for (uint8_t i=0; i<mLen; i++)
		{
			DUMP(mf->_fd.read());
			DUMP(' ');
		}
	  break;

	  case 0x59:// Key Signature
		DUMPS("KEY SIGNATURE ");
		for (uint8_t i=0; i<mLen; i++)
		{
			DUMP(mf->_fd.read());
			DUMP(' ');
		}
	  break;

	  case 0x7F:	// Sequencer Specific Metadata
		DUMPS("SEQ SPECIFIC ");
		for (uint8_t i=0; i<mLen; i++)
		{
			DUMPX(mf->_fd.read());
			DUMP(' ');
		}
	  break;
#endif // SHOW_UNUSED_META

	  default:
		mf->_fd.seekCur(mLen);
		DUMPS("IGNORED");
	  }
	  break;

// ---------------------------- UNKNOWN

	default:
		// stop playing this track as we cannot identify the eType
		_endOfTrack = true;	
		DUMPS("[UKNOWN 0x");
		DUMPX(eType);
		DUMPS("] Track aborted");
	}
}

int MD_MFTrack::load(uint8_t trackId, MD_MIDIFile *mf)
{
  uint32_t  dat32;
  uint16_t  dat16;

  // save the trackid for use later
  _trackId = _mev.track = trackId;
  
  // Read the Track header
  // track_chunk = "MTrk" + <length:4> + <track_event> [+ <track_event> ...]
  {
    #define HDR_SIZE  4
    char    h[HDR_SIZE+1]; // Header characters + nul
  
	mf->_fd.fgets(h, HDR_SIZE+1);
    h[HDR_SIZE] = '\0';

    if (strcmp(h, "MTrk") != 0)
      return(0);
  }

  // Row read track chunk size and in bytes. This is not really necessary 
  // since the track MUST end with an end of track meta event.
  dat32 = readMultiByte(&mf->_fd, MB_LONG);
  _length = dat32;

  // save where we are in the file as this is the start of offset for this track
  _startOffset = mf->_fd.curPosition();
  _currOffset = 0;

  // Advance the file pointer to the start of the next track;
  if (mf->_fd.seekSet(_startOffset+_length) == -1)
	  return(1);

  return(-1);
}

#if DUMP_DATA
void MD_MFTrack::dump(void)
{
	DUMPS("\n[Track ");
	DUMP(_trackId);
	DUMPS(" Header]");
	DUMPS("\nLength:\t\t\t");
	DUMP(_length);
	DUMPS("\nFile Location:\t\t");
    DUMP(_startOffset);
	DUMPS("\nEnd of Track:\t\t");
	DUMP(_endOfTrack);
	DUMPS("\nCurrent buffer offset:\t");
	DUMP(_currOffset);
}
#endif // DUMP_DATA

