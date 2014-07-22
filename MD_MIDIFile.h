/**
\mainpage Arduino Standard MIDI File (SMF) Player
MIDI (Musical Instrument Digital Interface) is a technical standard that describes a protocol, digital interface 
and connectors and allows a wide variety of electronic musical instruments, computers and other related devices 
to connect and communicate with one another. A single MIDI link can carry up to sixteen channels of information, 
each of which can be routed to a separate device.

MIDI carries event messages that specify notation, pitch and velocity, control signals for parameters such as 
volume, vibrato, audio panning, cues, and clock signals that set and synchronize tempo between multiple devices.
These messages are sent to other devices where they control sound generation and other features. This data can 
also be recorded into a hardware or software device called a sequencer, which can be used to edit the data and 
to play it back at a later time. These recordings are usually in Standard MIDI File (SMF) format.

Advantages of MIDI include compactness (an entire song can be coded in a few kilobytes), ease of modification 
and manipulation and choice of instruments.
 
This library allows SMF to be read from an SD card. They can be opened opened and processed - MIDI and SYSEX 
events are passed to the calling program through callback functions for processing. This allows the calling 
application to manage sending to a MIDI synthesizer through serial interface or other output device,
like a MIDI shield. SMF playing may be controlled through the library using methods to start, pause and restart 
playback.

External Dependencies
---------------------
The MD_MIDIFile library uses the following additional libraries and may need additional hardware components

- **SdFat library** at http://code.google.com/p/sdfatlib/. Used by the library to manage the SD card file system.

- **MIDI interface hardware** like http://www.stephenhobley.com/blog/2011/03/14/the-last-darned-midi-interface-ill-ever-build/.
The interface can be stand alone or built onto an Arduino shield, as required.

Topics
------
- \subpage pageSmfDefinition
- \subpage pageSoftware
- \subpage pageHardware

Revision History
----------------
##xx xxxx 2014 - version 2.0##
- Renamed MD_MIDIFile Library for consistency with other MajicDesigns libraries.
- Standardized
 + Documentation to Doxygen format.
 + Debug output macros.
- Fixed some minor errors.
- Added looping() method.

##20 January 2013 - version 1.2##
- Cleaned up examples and added additional comments.
- Removed dependency on the MIDI library. 
- This version has no major changes to the core library.

##6 January 2013 - version 1.1##
- Minor fixes and changes.
- More robust handling of file errors on MIDIFile.load().

##5 January 2013 - version 1.0##
- Initial release as MIDIFile Library.

Copyright
---------
Copyright (C) 2013-14 Marco Colli
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

\page pageSmfDefinition SMF Format Definition
Standard MIDI File Format
-------------------------
The Standard MIDI File (SMF) is a file format that provides a standardized way for 
sequences to be saved, transported, and opened in other systems. The compact size 
of these files has led to their widespread use in computers, mobile phone ring tones, 
web page authoring and greeting cards. They are intended for universal use, and include 
such information as note values, timing and track names. Lyrics may be included as 
metadata, and can be displayed by karaoke machines. The SMF specification was developed 
and is maintained by the MIDI Manufacturer's Association (MMA). 

SMFs are created as an export format of software sequencers or hardware workstations. 
They organize MIDI messages into one or more parallel tracks, and timestamp the events 
so that they can be played back in sequence. A SMF file is arranged into "chunks". It 
starts with a header chunk and is followed by one or more track chunks.

The header chunk contains the arrangement's setup data, which may include such things 
as tempo and instrumentation, and information such as the song's composer. The header 
also specifies which of three SMF formats applies to the file
- A __type 0__ file contains the entire performance, merged onto a single track
- A __type 1__ files may contain any number of tracks, running synchronously. 
- A __type 2__ files may contain any number of tracks, running asynchronously. This type
is rarely used and not supported by this library.

Each track chunk defines a logical track and contains events to be 
processed at specific time intervals. Events can be one of three types:

- __MIDI Events (status bytes 0x8n - 0xE0n)__

These correspond to the standard Channel MIDI messages. In this case '_n_' is the MIDI channel
(0 - 15). This status byte will be followed by 1 or 2 data bytes, as is usual for the 
particular MIDI message. Any valid Channel MIDI message can be included in a MIDI file.

If the first (status) byte is less than 128 (hex 80), this implies that running status is in 
effect, and that this byte is actually the first data byte (the status carrying over from the 
previous MIDI event). This can only be the case if the immediately previous event was also a 
MIDI event, ie SysEx and Meta events interrupt (clear) running status.

MIDI events may be processed by the calling program through a callback.

- __SYSEX events (status bytes 0xF0 and 0xF7)__

There are a couple of ways in which system exclusive messages can be encoded - as a single 
message (using the 0xF0 status), or split into packets (using the 0xF7 status). The 0xF7 
status is also used for sending escape sequences. 

SYSEX events may be processed by the calling program through a callback.

- __META events (status byte 0xFF)__

These contain additional information which would not be in the MIDI data stream itself. 
TimeSig, KeySig, Tempo, TrackName, Text, Marker, Special, EOT (End of Track) are the most 
common such events being some of the most common. 

Relevant META events are processed by the library code, but thid id only a subset of all 
the available events.

Note that the status bytes associated with System Common messages (0xF1 to 0xF6 inclusive) 
and System Real Time messages (0xF8 to 0xFE inclusive) are not valid within a MIDI file. 
For the rare occasion when you do need to include one of these messages, it should be embedded 
within a SysEx escape sequence.

SMF Format Grammar
------------------

This Grammar is from http://www.ccarh.org/courses/253/handout/smf/ and is usefule to understand 
the structure of the file in a programmer-friendly format.

     <descriptor:length> means 'length' bytes, MSB first
     <descriptor:v> means variable length argument (special format)

     SMF := <header_chunk> + <track_chunk> [ + <track_chunk> ... ]
     header chunk := "MThd" + <header_length:4> + <format:2> + <num_tracks:2> + <time_division:2>
     track_chunk := "MTrk" + <length:4> + <track_event> [ + <track_event> ... ]
     track_event := <time:v> + [ <midi_event> | <meta_event> | <sysex_event> ]
     midi_event := any MIDI channel message, including running status
     meta_event := 0xFF + <meta_type:1> + <length:v> + <event_data_bytes>
     sysex_event := 0xF0 + <len:1> + <data_bytes> + 0xF7 
     sysex_event := 0xF7 + <len:1> + <data_bytes> + 0xF7 


\page pageSoftware Software Library
The Library
-----------

Conditional Compilation Switches
--------------------------------

MIDI Timing
-----------


\page pageHardware Hardware Interface

The MIDI communications hardware is a opt-isolated byte-based serial interface configured 
at 31,250 baud (bps). The standard connector is a 5 pin DIN, however other types of 
connections (like MIDI over USB) have become increasingly common as other interfaces 
that had been used for MIDI connections (serial, joystick, etc.) disappeared as standard 
features of personal computers.

Standard MIDI IN, OUT and THRU Ports
------------------------------------
Standard MIDI cables terminate in a 180° five-pin DIN connector. Standard applications 
use only three of the five conductors: a ground wire, and a balanced pair of conductors 
that carry a +5 volt signal. This connector configuration can only carry messages in one 
direction, so a second cable is necessary for two-way communication. These are usually 
labeled IN and OUT designating the messages going into and coming out of the device, 
respectively.

Most devices do not copy messages from their IN to their OUT port. A third type of 
port, the THRU port, emits a copy of everything received at the input port, allowing 
data to be forwarded to another instrument in a daisy-chain arrangement. Not all devices 
contain THRU ports, and devices that lack the ability to generate MIDI data, such as 
effects units and sound modules, may not include OUT ports.

Opto-isolators keep MIDI devices electrically separated from their connectors, which 
prevents the occurrence of ground loops and protects equipment from voltage spikes. There 
is no error detection capability in MIDI, so the maximum cable length is set at 15 meters 
(50 feet) in order to limit interference.

A circuit using the Arduino Serial interface is shown below. This can be built as a stand-alone 
interface or incorporated into an Arduino shield. This is based on the device described at 
http://www.stephenhobley.com/blog/2011/03/14/the-last-darned-midi-interface-ill-ever-build/.

![MIDI Interface Circuit] (MIDI_Interface.jpg "MIDI Interface Circuit")

*/

#ifndef _MDMIDIFILE_H
#define _MDMIDIFILE_H

#include <Arduino.h>
#include <SdFat.h>

/**
 * \file
 * \brief Main header file for the MD_MIDIFile library
 */

// ------------- Configuration Section - START

/**
 \def DUMP_DATA
 Set to 1 to to dump the file data instead of processing callback.
 Neither of DUMP_DATA or SHOW_UNUSED_META should be 1 when MIDI messages are being 
 transmitted as the Serial.print() functions are called to print information.
 */
#define	DUMP_DATA			    0

/**
 \def SHOW_UNUSED_META
 Set to 1 to display unused META messages. DUMP_DATA must also be enabled.
 Neither of DUMP_DATA or SHOW_UNUSED_META should be 1 when MIDI messages are being 
 transmitted as the Serial.print() functions are called to print information.
 */
#define	SHOW_UNUSED_META	0

/**
 \def MIDI_MAX_TRACKS
 Max number of MIDI tracks. This may be reduced or increased depending on memory requirements.
 16 tracks is the maximum available to any MIDI device. Fewer tracks may not allow many MIDI
 files to be played, while a minority of SMF may require more tracks.
 */
#define MIDI_MAX_TRACKS 16

// ------------- Configuration Section - END

#if DUMP_DATA
#define	DUMPS(s)    Serial.print(F(s))                            ///< Print a string
#define	DUMP(s, v)	{ Serial.print(F(s)); Serial.print(v); }      ///< Print a value (decimal)
#define	DUMPX(s, x)	{ Serial.print(F(s)); Serial.print(x,HEX); }  ///< Print a value (hex)
#else
#define	DUMPS(s)      ///< Print a string
#define	DUMP(s, v)    ///< Print a value (decimal)
#define	DUMPX(s, x)   ///< Print a value (hex)
#endif // DUMP_DATA

/**
 * Structure defining a MIDI event and its related data.
 * A pointer to this structure type is passed the the related callback function.
 */
typedef struct
{
	uint8_t	track;		///< the track this was on
	uint8_t	channel;	///< the midi channel
	uint8_t	size;		  ///< the number of data bytes
	uint8_t	data[4];	///< the data. Only 'size' bytes are valid
} midi_event;

/**
 * Structure defining a SYSEX event and its related data.
 * A pointer to this structure type is passed the the related callback function.
 */
typedef struct
{
	uint8_t	track;		///< the track this was on
	uint8_t	size;		  ///< the number of data bytes
	uint8_t	data[50];	///< the data. Only 'size' bytes are valid
} sysex_event;


class MD_MIDIFile;

/**
 * Object definition defining a MIDI track
 */
class MD_MFTrack 
{
  public:
    MD_MFTrack(void);
    ~MD_MFTrack(void);

	// Track header file data
	uint32_t	getLength(void);		  // get length of track in bytes
	bool		  getEndOfTrack(void);	// get end of track status (true = EOT)

	// Data handling
	void	syncTime(void);				// reset the start time for this track
	void	restart(void);				// reset the track to the start of data in the file
	int		load(uint8_t trackId, MD_MIDIFile *mf); // load the definition for a track
	void	close(void);					// close a track
	bool	getNextEvent(MD_MIDIFile *mf, uint32_t elapsedTime);	// get and process the events that need to be processed
#if DUMP_DATA
	void	dump(void);					  // DUMP the data - used for debugging
#endif // DUMP_DATA

  protected:
	void		parseEvent(MD_MIDIFile *mf);	// process the event from the physical file
	void		reset(void);	      // initialize class variables all in one place

	uint8_t		_trackId;		      // the id for this track
  uint32_t	_length;          // length of track in bytes
  uint32_t	_startOffset;     // start of the track in bytes from start of file
	uint32_t	_currOffset;	    // offset from start of the track for the next read of SD data
	bool		  _endOfTrack;	    // true when we have reached end of track or we have encountered an undefined event
	uint32_t	_elapsedTimeTotal;	// the total time elapsed in microseconds since events were checked
	midi_event  _mev;			      // data for MIDI callback function - persists between calls for run-on messages
};

/**
 * Core object for the MD_MIDIFile library.
 * This is the class to handle a MIDI file, including all tracks, and is the only one 
 * available to user programs.
 */
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
	void	  restart(void);		  // reset to the start of all tracks
  void    looping(bool bMode);// set looping mode when true and reset when false
    
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
  void    calcMicrosecondDelta(void);	// called internally to update the time per tick
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
  bool    _looping;       // if true we are currently looping

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
