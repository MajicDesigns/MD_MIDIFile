/*
  MD_MIDIFile.h - An Arduino library for processing Standard MIDI Files (SMF).
  Copyright (C) 2012 Marco Colli
  All rights reserved.

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

  External dependencies
  =====================
  SdFat library at http://code.google.com/p/sdfatlib/
	Used by the library to read SMF from the the SD card.
  MIDI interface hardware like 
    http://www.stephenhobley.com/blog/2011/03/14/the-last-darned-midi-interface-ill-ever-build/
    The interface can be stand alone or built onto an Arduino shield, as required.

  Standard MIDI File Format
  =========================
  A standard MIDI file is composed of "chunks". It starts with a header chunk and is 
  followed by one or more track chunks. The header chunk contains data that pertains to 
  the overall file. Each track chunk defines a logical track and contains events to be 
  processed at specific time intervals. Events can be one of three types:

  * MIDI events (status bytes 0x8n - 0xEn)
	Corresponding to the standard Channel MIDI messages, ie where 'n' is the MIDI channel (0 - 15). 
	This status byte will be followed by 1 or 2 data bytes, as is usual for the particular MIDI 
	message. Any valid Channel MIDI message can be included in a MIDI file.
	If the first (status) byte is less than 128 (hex 80), this implies that running status is in 
	effect, and that this byte is actually the first data byte (the status carrying over from the 
	previous MIDI event). This can only be the case if the immediately previous event was also a 
	MIDI event, ie SysEx and Meta events interrupt (clear) running status.
	MIDI events may be processed by the calling program through a callback.

  * SYSEX events (status bytes 0xF0 and 0xF7)
	There are a couple of ways in which system exclusive messages can be encoded - as a single 
	message (using the 0xF0 status), or split into packets (using the 0xF7 status). The 0xF7 
	status is also used for sending escape sequences.
	SYSEX events may be processed by the calling program through a callback.

  * META events (status byte 0xFF)
	These contain additional information which would not be in the MIDI data stream itself. 
	TimeSig, KeySig, Tempo, TrackName, Text, Marker, Special, EOT (End of Track) are the most 
	common such events being some of the most common.
	META events are processed by the library code. Only a subset of all the available events 
	are processed.

	Note that the status bytes associated with System Common messages (0xF1 to 0xF6 inclusive) 
	and System Real Time messages (0xF8 to 0xFE inclusive) are not valid within a MIDI file. 
	Generally none of these messages are relevant within a MIDI file, though for the rare 
	occasion when you do need to include one, it should be embedded within a SysEx escape 
	sequence.

  Abbreviated Descrption (from http://www.ccarh.org/courses/253/handout/smf/)
  ======================
  <descriptor:length> means 'length' bytes, MSB first
  <descriptor:v> means variable length argument (special format)

  SMF := <header_chunk> + <track_chunk> [+ <track_chunk> ...]
  header chunk := "MThd" + <header_length:4> + <format:2> + <num_tracks:2> + <time_division:2>
  track_chunk := "MTrk" + <length:4> + <track_event> [+ <track_event> ...]
  track_event := <time:v> + [<midi_event> | <meta_event> | <sysex_event>]
  midi_event := any MIDI channel message, including running status
  meta_event := 0xFF + <meta_type:1> + <length:v> + <event_data_bytes>
  sysex_event := 0xF0 + <len:1> + <data_bytes> + 0xF7 
  sysex_event := 0xF7 + <len:1> + <data_bytes> + 0xF7 
*/

#ifndef _MDMIDIFILE_H
#define _MDMIDIFILE_H

#include <Arduino.h>
#include <SdFat.h>

// ------------- Configuration Section - START
//
// Neither of DUMP_DATA or SHOW_UNUSED_META should be 1 when MIDI messages are being 
// transmitted as the Serial.print() functions are called to print information.
//
#define	DUMP_DATA			    0 // Set to 1 to dump the file data instead of processing callback
#define	SHOW_UNUSED_META	0	// Set to 1 to display unused META messages - DUMP_DATA must be on
//
// Max number of MIDI tracks. This may be reduced or increased depending on memory requirements.
// 16 tracks is the maximum available to any MIDI device. Fewer tracks may not allow many MIDI 
// files to be played, while a minority of files may require more tracks.
//
#define MIDI_MAX_TRACKS 16
//
// ------------- Configuration Section - END

#if DUMP_DATA
#define	DUMPS(x)	Serial.print(F(x))
#define	DUMP(x)		Serial.print((x))
#define	DUMPX(x)	Serial.print((x),HEX)
#else
#define	DUMPS(x)
#define	DUMP(x)
#define	DUMPX(x)
#endif // DUMP_DATA

// Class and data structure definitions
typedef struct
{
	uint8_t	track;		// the track this was on
	uint8_t	channel;	// the midi channel
	uint8_t	size;		// the number of data bytes
	uint8_t	data[4];	// the data. Only 'size' bytes are valid
} midi_event;

typedef struct
{
	uint8_t	track;		// the track this was on
	uint8_t	size;		// the number of data bytes
	uint8_t	data[50];	// the data. Only 'size' bytes are valid
} sysex_event;

// MIDIFile is the class to handle a MIDI file, including all tracks.
// This class is the only one available to user programs.

class MD_MIDIFile;

// MFTrack is the class to handle a MIDI track chunk
class MD_MFTrack 
{
  public:
    MD_MFTrack(void);
    ~MD_MFTrack(void);

	// Track header file data
	uint32_t	getLength(void);		// get length of track in bytes
	bool		getEndOfTrack(void);	// get end of track status (true = EOT)

	// Data handling
	void	syncTime(void);				// reset the start time for this track
	void	restart(void);				// reset the track to the start of data in the file
	int		load(uint8_t trackId, MD_MIDIFile *mf);// load the definition for a track
	void	close(void);						// close a track
	bool	getNextEvent(MD_MIDIFile *mf, uint32_t elapsedTime);	// get and process the events that need to be processed
#if DUMP_DATA
	void	dump(void);					// DUMP the data - used for debugging
#endif // DUMP_DATA

  protected:
	void		parseEvent(MD_MIDIFile *mf);	// process the event from the physical file
	void		reset(void);	// Initialise class variables all in one place

	uint8_t		_trackId;		// the id for this track
  uint32_t	_length;        // length of track in bytes
  uint32_t	_startOffset;   // start of the track in bytes from start of file
	uint32_t	_currOffset;	// offset from start of the track for the next read of SD data
	bool		  _endOfTrack;	// true when we have reached end of track or we have encountered an undefined event
	uint32_t	_elapsedTimeTotal;	// the total time elapsed in microseconds since events were checked
	midi_event  _mev;			// data for MIDI callback function - persists between calls for run-on messages
};

class MD_MIDIFile 
{
  public:
	friend class MD_MFTrack;

  // Constructor / destructor
  MD_MIDIFile(void);
  ~MD_MIDIFile(void);
	void	  begin(SdFat *psd);	// pass in the SD structure for use with the library

  // MIDI time base in the file
  uint16_t  getTicksPerQuarterNote(void);
  void      setTicksPerQuarterNote(uint16_t ticks);
	uint16_t  getTempo(void);
	void	    setTempo(uint16_t t);
	uint16_t  getTimeSignature(void);
	void	    setTimeSignature(uint8_t n, uint8_t d);
  uint32_t  getMicrosecondDelta(void);
	void	    setMicrosecondPerQuarterNote(uint32_t m);

  // MIDI file name
  void      setFilename(const char* aname);
  const char* getFilename(void);
    
  // MIDI file header data
  uint8_t   getFormat(void);
  uint8_t   getTrackCount(void);

	// Playback control
	void	  pause(bool bMode);	// start pause when true and restart when false
	void	  restart(void);		// reset to the start of all tracks
    
    // Data handling
  int       load(void);			// 'Open & Load' the file ready for playing. Error codes returned:
									// -1 = All good
									//  0 = Blank file name
									//  2 = Can't open file specified
									//  3 = File is not MIDI format
									//  4 = MIDI header size incorrect
									//  5 = File format type not 0 or 1
									//  6 = File format 0 but more than 1 track
									//  7 = More than MIDI_MAX_TRACKS required
									// n0 = Track n track chunk not found
									// n1 = Track n chunk size past end of file

  void      close(void);			// 'Close' the file
	boolean	  getNextEvent(void);	// get and process the events that need to be processed. return true is a 'tick' has passed.
	bool	    isEOF(void);			// return true if end of track has been reached for all tracks (ie, nothing left to play)
	void	    setMidiHandler(void (*mh)(midi_event *pev));		// set the data handling callback for MIDI messages
	void	    setSysexHandler(void (*sh)(sysex_event *pev));	// set the data handling callback for SYSEX messages
	void	    dump(void);			// DUMP the data - used for debugging

  protected:
  void    calcMicrosecondDelta(void);			// called internally to update the time per tick
	void	  initialise(void);						// initialise class variables all in one place
	void	  synchTracks(void);					// synchronise the start of all tracks

	void	  (*_midiHandler)(midi_event *pev);		// callback into user code to process MIDI stream
	void	  (*_sysexHandler)(sysex_event *pev);	// callback into user code to process SYSEX stream

	char      _fileName[13];        // MIDI file name - should be 8.3 format

  uint8_t   _format;              // file format - 0: single track, 1: multiple track, 2: multiple song
  uint8_t   _trackCount;          // # of tracks in file
 
	// The MIDI header chunk contains a 16-bit value that gives the number of ticks per quarter note. 
	// (Ticks are the units measured by the delta-time values). This value is a constant over the 
	// whole file. Within the MIDI data stream are tempo meta-events, which contain a 24-bit value 
	// that give the number of microseconds per quarter note. Divide this one by the first one, 
	// and you get the number of microseconds per tick.
	// MIDI default is 48.
	uint16_t  _ticksPerQuarterNote; // time base of file
	uint32_t  _microsecondDelta;	// calculated per tick based on other data for MIDI file
	uint32_t  _lastEventCheckTime;	// the last time an event check was performed
	bool	  _syncAtStart;			// sync up at the start of all tracks
	bool	  _paused;				// if true we are currently paused

	// The fundamental time unit of music is the beat. Beats can be slower or faster depending 
	// on the kind of music, and the tempo (speed of the beats) can change even in a single 
	// piece. Tempos in standard music notation are typically given in beats per minute.
	// MIDI default is 120.
 	uint16_t  _tempo;				// tempo for this file in beats per minute

	// Notes come in different power-of-two lengths. A quarter note normally is one beat long 
	// (although this isn't always the case). A half note is two beats, and a whole note is 
	// four beats. (It's called a whole note because it takes up a whole measure, if you're in 4.)
	// An eighth note is half a quarter note, so there are two eighth notes per beat, 
	// a sixteenth note is half an eighth so there are 4 sixteenths per beat, and so on.
	// MIDI default is 4/4.
	uint8_t   _timeSignature[2];	// time signature [0] = numerator, [1] = denominator

	// file handling
    uint8_t   _selectSD;            // SDFat select line
    SdFat     *_sd;		            // SDFat library descriptor supplied by calling program
    SdFile    _fd;                  // SDFat file descriptor
    MD_MFTrack   _track[MIDI_MAX_TRACKS];      // the track data for this file
};

#endif /* _MDMIDIFILE_H */
