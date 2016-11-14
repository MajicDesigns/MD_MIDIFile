/*
  MD_MIDIFile.cpp - An Arduino library for processing Standard MIDI Files (SMF).
  Copyright (C) 2012 Marco Colli
  All rights reserved.

  See MD_MIDIFile.h for complete comments

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

/**
 * \file
 * \brief Main file for the MD_MIDIFile class implementation
 */

void MD_MIDIFile::initialise(void)
{
  _trackCount = 0;            // number of tracks in file
  _format = 0;
  _tickTime = 0;
  _lastTickError = 0;
  _syncAtStart = false;
  _paused =_looping = false;
  
  setMidiHandler(NULL);
  setSysexHandler(NULL);

  // File handling
  setFilename("");
  _sd = NULL;

  // Set MIDI defaults
  setTicksPerQuarterNote(48); // 48 ticks per quarter note
  setTempo(120);				      // 120 beats per minute
  setTempoAdjust(0);          // 0 beats per minute adjustment
  setMicrosecondPerQuarterNote(500000);  // 500,000 microseconds per quarter note
  setTimeSignature(4, 4);		  // 4/4 time
}

void MD_MIDIFile::synchTracks(void)
{
	for (uint8_t i=0; i<_trackCount; i++)
		_track[i].syncTime();
    
	_lastTickCheckTime = micros();
}

MD_MIDIFile::MD_MIDIFile(void) 
{ 
	initialise();
}

MD_MIDIFile::~MD_MIDIFile() 
{ 
  close();
}

void MD_MIDIFile::begin(SdFat *psd)
{
	_sd = psd;
}

void MD_MIDIFile::close()
// Close out - should be ready for the next file
{
  for (uint8_t i = 0; i<_trackCount; i++)
  {
	  _track[i].close();
  }
  _trackCount = 0;
  _syncAtStart = false;
  _paused = false;

  setFilename("");
  _fd.close();
}

void MD_MIDIFile::setFilename(const char* aname) 
// sets the filename of the MIDI file.
// expects it to be in 8.3 format.
{
	if (aname != NULL)
		strcpy(_fileName, aname);
}

const char* MD_MIDIFile::getFilename(void) 
// returns the name of the current file
{
  return _fileName;
}

uint8_t MD_MIDIFile::getTrackCount(void) 
// return the number of tracks in the Midi File.
{ 
  return _trackCount;
}

uint8_t MD_MIDIFile::getFormat(void) 
// return the format of the MIDI File.
{ 
  return _format;
}

int16_t MD_MIDIFile::getTempoAdjust(void)
{
  return _tempoDelta;
}

uint16_t MD_MIDIFile::getTempo(void)
{
  return _tempo;
}

void MD_MIDIFile::setTempoAdjust(int16_t t)
{
  if ((t + _tempo) > 0) _tempoDelta = t;
  calcTickTime();
}

void MD_MIDIFile::setTempo(uint16_t t)
{
  if ((_tempoDelta + t) > 0) _tempo = t;
  calcTickTime();
}

uint16_t MD_MIDIFile::getTimeSignature(void)
{
  return ((_timeSignature[0]<<8) + _timeSignature[1]);
}

void MD_MIDIFile::setTimeSignature(uint8_t n, uint8_t d)
{
  _timeSignature[0] = n;
  _timeSignature[1] = d;
  calcTickTime();
}

void MD_MIDIFile::setTicksPerQuarterNote(uint16_t ticks) 
{
  _ticksPerQuarterNote = ticks;
  calcTickTime();
}

uint16_t MD_MIDIFile::getTicksPerQuarterNote(void) 
{ 
  return _ticksPerQuarterNote;
}

void MD_MIDIFile::setMicrosecondPerQuarterNote(uint32_t m)
// This is the value given in the META message setting tempo
{
  // work out the tempo from the delta by reversing the calcs in
  // calctickTime - m is already per quarter note
  _tempo = (60 * 1000000L) / m;
  calcTickTime();
}

void MD_MIDIFile::calcTickTime(void) 
// 1 tick = microseconds per beat / ticks per Q note
// The variable "microseconds per beat" is specified by a MIDI event carrying 
// the set tempo meta message. If it is not specified then it is 500,000 microseconds 
// by default, which is equivalent to 120 beats per minute. 
// If the MIDI time division is 60 ticks per beat and if the microseconds per beat 
// is 500,000, then 1 tick = 500,000 / 60 = 8333.33 microseconds.
{
  if ((_tempo + _tempoDelta != 0) && _ticksPerQuarterNote != 0 && _timeSignature[1] != 0)
	{
		_tickTime = (60 * 1000000L) / (_tempo + _tempoDelta); // microseconds per beat
		_tickTime = (_tickTime * 4) / (_timeSignature[1] * _ticksPerQuarterNote);	// microseconds per tick
	}
}

uint32_t MD_MIDIFile::getTickTime(void) 
{
	return _tickTime;
}

void MD_MIDIFile::setMidiHandler(void (*mh)(midi_event *pev))
{
	_midiHandler = mh;
}

void MD_MIDIFile::setSysexHandler(void (*sh)(sysex_event *pev))
{
	_sysexHandler = sh;
}

bool MD_MIDIFile::isEOF(void)
{
  bool bEof = true;
  
  // check if each track has finished
	for (uint8_t i=0; i<_trackCount && bEof; i++)
	{
		 bEof = (_track[i].getEndOfTrack() && bEof);  // breaks at first false
	}
  
  if (bEof) DUMPS("\n! EOF");
   
  // if looping and all tracks done, reset to the start
  if (bEof && _looping)
  {
    restart();
    bEof = false;
  }
   
	 return(bEof);
}

void MD_MIDIFile::pause(bool bMode)
// Start pause when true and restart when false
{
	_paused = bMode;

	if (!_paused)				    // restarting so ..
		_syncAtStart = false;	// .. force a time resynch when next processing events
}

void MD_MIDIFile::restart(void)
// Reset the file to the start of all tracks
{
  // track 0 contains information that does not need to be reloaded every time, 
  // so if we are looping, ignore restarting that track. The file may have one 
  // track only and in this case always sync from track 0.
	for (uint8_t i=(_looping && _trackCount>1 ? 1 : 0); i<_trackCount; i++)
		_track[i].restart();

	_syncAtStart = false;		// force a time resych
}

void MD_MIDIFile::looping(bool bMode)
{
  _looping = bMode;
}

uint16_t MD_MIDIFile::tickClock(void)
// check if enough time has passed for a MIDI tick and work out how many!
{
	uint32_t	elapsedTime;
  uint16_t   ticks = 0;
  
	elapsedTime = _lastTickError + micros() - _lastTickCheckTime;
	if (elapsedTime >= _tickTime)
  {
    ticks = elapsedTime/_tickTime;
    _lastTickError = elapsedTime - (_tickTime * ticks);
		_lastTickCheckTime = micros();			// save for next round of checks
  }  
 
  return(ticks);
}

boolean MD_MIDIFile::getNextEvent(void)
{
	uint16_t		ticks;

	// if we are paused we are paused!
	if (_paused) 
    return false;

	// sync start all the tracks if we need to
	if (!_syncAtStart)
	{
		synchTracks();
		_syncAtStart = true;
	}

	// check if enough time has passed for a MIDI tick
  if ((ticks = tickClock()) == 0)
		return false;	

  processEvents(ticks);
  
  return(true);
}

void MD_MIDIFile::processEvents(uint16_t ticks)
{
	uint8_t		n;

	if (_format != 0) 
  {
    DUMP("\n-- [", ticks); 
    DUMPS("] TRK "); 
  }    

#if TRACK_PRIORITY
	// process all events from each track first - TRACK PRIORITY
	for (uint8_t i = 0; i < _trackCount; i++)
	{
		if (_format != 0) DUMPX("", i);
		// Limit n to be a sensible number of events in the loop counter
		// When there are no more events, just break out
		// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
		for (n=0; n < 100; n++)
		{
			if (!_track[i].getNextEvent(this, (n==0 ? ticks : 0)))
				break;
		}

		if ((n > 0) && (_format != 0))
			DUMPS("\n-- TRK "); 
	}
#else // EVENT_PRIORITY
	// process one event from each track round-robin style - EVENT PRIORITY
	bool doneEvents;

	// Limit n to be a sensible number of events in the loop counter
	for (n = 0; (n < 100) && (!doneEvents); n++)
	{
		doneEvents = false;

		for (uint8_t i = 0; i < _trackCount; i++)	// cycle through all
		{
			bool	b;

			if (_format != 0) DUMPX("", i);

			// Other than the first event, the other have an elapsed time of 0 (ie occur simultaneously)
			b = _track[i].getNextEvent(this, (n==0 ? ticks : 0));
			if (b && (_format != 0))
				DUMPS("\n-- TRK "); 
			doneEvents = (doneEvents || b);
		}

		// When there are no more events, just break out
		if (!doneEvents)
			break;
	} 
#endif // EVENT/TRACK_PRIORITY
}

int MD_MIDIFile::load() 
// Load the MIDI file into memory ready for processing
{
  uint32_t  dat32;
  uint16_t  dat16;
  
  if (_fileName[0] == '\0')  
    return(0);

  // open the file for reading:
  if (!_fd.open(_fileName, O_READ)) 
    return(2);

  // Read the MIDI header
  // header chunk = "MThd" + <header_length:4> + <format:2> + <num_tracks:2> + <time_division:2>
  {
    char    h[MTHD_HDR_SIZE+1]; // Header characters + nul
  
    _fd.fgets(h, MTHD_HDR_SIZE+1);
    h[MTHD_HDR_SIZE] = '\0';
    
    if (strcmp(h, MTHD_HDR) != 0)
	  {
	    _fd.close();
      return(3);
	  }
  }
  
  // read header size
  dat32 = readMultiByte(&_fd, MB_LONG);
  if (dat32 != 6)   // must be 6 for this header
  {
	  _fd.close();
    return(4);
  }
  
  // read file type
  dat16 = readMultiByte(&_fd, MB_WORD);
  if ((dat16 != 0) && (dat16 != 1))
  {
	  _fd.close();
    return(5);
  }
  _format = dat16;
 
   // read number of tracks
  dat16 = readMultiByte(&_fd, MB_WORD);
  if ((_format == 0) && (dat16 != 1)) 
  {
	_fd.close();
    return(6);
  }
  if (dat16 > MIDI_MAX_TRACKS)
  {
	_fd.close();
	return(7);
  }
  _trackCount = dat16;

   // read ticks per quarter note
  dat16 = readMultiByte(&_fd, MB_WORD);
  if (dat16 & 0x8000) // top bit set is SMTE format
  {
      int framespersecond = (dat16 >> 8) & 0x00ff;
      int resolution      = dat16 & 0x00ff;

      switch (framespersecond) {
        case 232:  framespersecond = 24; break;
        case 231:  framespersecond = 25; break;
        case 227:  framespersecond = 29; break;
        case 226:  framespersecond = 30; break;
        default:   _fd.close();	return(7);
      }
      dat16 = framespersecond * resolution;
   } 
   _ticksPerQuarterNote = dat16;
   calcTickTime();	// we may have changed from default, so recalculate

   // load all tracks
   for (uint8_t i = 0; i<_trackCount; i++)
   {
	   int err;
	   
	   if ((err = _track[i].load(i, this)) != -1)
	   {
		   _fd.close();
		   return((10*(i+1))+err);
	   }
   }

  return(-1);
}

#if DUMP_DATA
void MD_MIDIFile::dump(void)
{
  DUMP("\nFile Name:\t", getFilename());
  DUMP("\nFile format:\t", getFormat());
  DUMP("\nTracks:\t\t", getTrackCount());
  DUMP("\nTime division:\t", getTicksPerQuarterNote());
  DUMP("\nTempo:\t\t", getTempo());
  DUMP("\nMicrosec/tick:\t", getTickTime());
  DUMP("\nTime Signature:\t", getTimeSignature()>>8);
  DUMP("/", getTimeSignature() & 0xf);
  DUMPS("\n");
 
  for (uint8_t i=0; i<_trackCount; i++)
  {
	  _track[i].dump();
	  DUMPS("\n");
  } 
}
#endif // DUMP_DATA

