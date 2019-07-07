/*
  MD_MIDITrack.cpp - An Arduino library for processing Standard MIDI Files (SMF).
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
 * \brief Main file for the MFTrack class implementation
 */

void MD_MFTrack::reset(void)
{
  _length = 0;        // length of track in bytes
  _startOffset = 0;   // start of the track in bytes from start of file
  restart();
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
  _elapsedTicks = 0;
}

void MD_MFTrack::restart(void)
// Start playing the track from the beginning again
{
  _currOffset = 0;
  _endOfTrack = false;
  _elapsedTicks = 0;
}

bool MD_MFTrack::getNextEvent(MD_MIDIFile *mf, uint16_t tickCount)
// track_event = <time:v> + [<midi_event> | <meta_event> | <sysex_event>]
{
  uint32_t deltaT;

  // is there anything to process?
  if (_endOfTrack)
    return(false);

  // move the file pointer to where we left off
  mf->_fd.seekSet(_startOffset+_currOffset);

  // Work out new total elapsed ticks - include the overshoot from
  // last event.
  _elapsedTicks += tickCount;

  // Get the DeltaT from the file in order to see if enough ticks have
  // passed for the event to be active.
  deltaT = readVarLen(&mf->_fd);

  // If not enough ticks, just return without saving the file pointer and 
  // we will go back to the same spot next time.
  if (_elapsedTicks < deltaT)
    return(false);

  // Adjust the total elapsed time to the error against actual DeltaT to avoid 
  // accumulation of errors, as we only check for _elapsedTicks being >= ticks,
  // giving positive biased errors every time.
  _elapsedTicks -= deltaT;

  DUMP("\ndT: ", deltaT);
  DUMP(" + ", _elapsedTicks);
  DUMPS("\t");

  parseEvent(mf);

  // remember the offset for next time
  _currOffset = mf->_fd.curPosition() - _startOffset;

  // catch end of track when there is no META event  
  _endOfTrack = _endOfTrack || (_currOffset >= _length);
  if (_endOfTrack) DUMPS(" - OUT OF TRACK");

  return(true);
}

void MD_MFTrack::parseEvent(MD_MIDIFile *mf)
// process the event from the physical file
{
  uint8_t eType;
  uint32_t eLen, mLen;

  // now we have to process this event
  eType = mf->_fd.read();

  switch (eType)
  {
// ---------------------------- MIDI
    // midi_event = any MIDI channel message, including running status
    // Midi events (status bytes 0x8n - 0xEn) The standard Channel MIDI messages, where 'n' is the MIDI channel (0 - 15).
    // This status byte will be followed by 1 or 2 data bytes, as is usual for the particular MIDI message. 
    // Any valid Channel MIDI message can be included in a MIDI file.
  case 0x80 ... 0xBf: // MIDI message with 2 parameters
  case 0xe0 ... 0xef:
    _mev.size = 3;
    _mev.data[0] = eType;
    _mev.channel = _mev.data[0] & 0xf;  // mask off the channel
    _mev.data[0] = _mev.data[0] & 0xf0; // just the command byte
    _mev.data[1] = mf->_fd.read();
    _mev.data[2] = mf->_fd.read();
    DUMP("[MID2] Ch: ", _mev.channel);
    DUMPX(" Data: ", _mev.data[0]);
    DUMPX(" ", _mev.data[1]);
    DUMPX(" ", _mev.data[2]);
#if !DUMP_DATA
    if (mf->_midiHandler != nullptr)
      (mf->_midiHandler)(&_mev);
#endif // !DUMP_DATA
  break;

  case 0xc0 ... 0xdf: // MIDI message with 1 parameter
    _mev.size = 2;
    _mev.data[0] = eType;
    _mev.channel = _mev.data[0] & 0xf;  // mask off the channel
    _mev.data[0] = _mev.data[0] & 0xf0; // just the command byte
    _mev.data[1] = mf->_fd.read();
    DUMP("[MID1] Ch: ", _mev.channel);
    DUMPX(" Data: ", _mev.data[0]);
    DUMPX(" ", _mev.data[1]);

#if !DUMP_DATA
    if (mf->_midiHandler != nullptr)
      (mf->_midiHandler)(&_mev);
#endif
  break;

  case 0x00 ... 0x7f: // MIDI run on message
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
      _mev.data[i] = mf->_fd.read();  // next byte
    } 

    DUMP("[MID+] Ch: ", _mev.channel);
    DUMPS(" Data:");
    for (uint8_t i = 0; i<_mev.size; i++)
    {
      DUMPX(" ", _mev.data[i]);
    }

#if !DUMP_DATA
    if (mf->_midiHandler != nullptr)
      (mf->_midiHandler)(&_mev);
#endif
  }
  break;

// ---------------------------- SYSEX
  case 0xf0:  // sysex_event = 0xF0 + <len:1> + <data_bytes> + 0xF7 
  case 0xf7:  // sysex_event = 0xF7 + <len:1> + <data_bytes> + 0xF7 
  {
    sysex_event sev;
    uint16_t index = 0;

    // collect all the bytes until the 0xf7 - boundaries are included in the message
    sev.track = _trackId;
    mLen = readVarLen(&mf->_fd);
    sev.size = mLen;
    if (eType==0xF0)       // add space for 0xF0
    {
      sev.data[index++] = eType;
      sev.size++;
    }
    uint16_t minLen = min(sev.size, ARRAY_SIZE(sev.data));
    // The length parameter includes the 0xF7 but not the start boundary.
    // However, it may be bigger than our buffer will allow us to store.
    for (uint16_t i=index; i<minLen; ++i)
      sev.data[i] = mf->_fd.read();
    if (sev.size>minLen)
      mf->_fd.seekCur(sev.size-minLen);

#if DUMP_DATA
    DUMPS("[SYSX] Data:");
    for (uint16_t i = 0; i<minLen; i++)
    {
      DUMPX(" ", sev.data[i]);
    }
    if (sev.size>minLen)
      DUMPS("...");
#else
    if (mf->_sysexHandler != nullptr)
      (mf->_sysexHandler)(&sev);
#endif
  }
  break;

// ---------------------------- META
  case 0xff:  // meta_event = 0xFF + <meta_type:1> + <length:v> + <event_data_bytes>
  {
    meta_event mev;

    eType = mf->_fd.read();
    mLen =  readVarLen(&mf->_fd);

    mev.track = _trackId;
    mev.size = mLen;
    mev.type = eType;

    DUMPX("[META] Type: 0x", eType);
    DUMP("\tLen: ", mLen);
    DUMPS("\t");

    switch (eType)
    {
      case 0x2f:  // End of track
      {
        _endOfTrack = true;
        DUMPS("END OF TRACK");
      }
      break;

      case 0x51:  // set Tempo - really the microseconds per tick
      {
        uint32_t value = readMultiByte(&mf->_fd, MB_TRYTE);
        
        mf->setMicrosecondPerQuarterNote(value);
        
        mev.data[0] = (value >> 16) & 0xFF;
        mev.data[1] = (value >> 8) & 0xFF;
        mev.data[2] = value & 0xFF;
        
        DUMP("SET TEMPO to ", mf->getTickTime());
        DUMP(" us/tick or ", mf->getTempo());
        DUMPS(" beats/min");
      }
      break;

      case 0x58:  // time signature
      {
        uint8_t n = mf->_fd.read();
        uint8_t d = mf->_fd.read();
        
        mf->setTimeSignature(n, 1 << d);  // denominator is 2^n
        mf->_fd.seekCur(mLen - 2);

        mev.data[0] = n;
        mev.data[1] = d;
        mev.data[2] = 0;
        mev.data[3] = 0;

        DUMP("SET TIME SIGNATURE to ", mf->getTimeSignature() >> 8);
        DUMP("/", mf->getTimeSignature() & 0xf);
      }
      break;

      case 0x59:  // Key Signature
      {
        DUMPS("KEY SIGNATURE");
        int8_t sf = mf->_fd.read();
        uint8_t mi = mf->_fd.read();
        const char* aaa[] = {"Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#"};

        if (sf >= -7 && sf <= 7) 
        {
          switch(mi)
          {
            case 0:
              strcpy(mev.chars, aaa[sf+7]);
              strcat(mev.chars, "M");
              break;
            case 1:
              strcpy(mev.chars, aaa[sf+10]);
              strcat(mev.chars, "m");
              break;
            default:
              strcpy(mev.chars, "Err"); // error mi
          }
        } else
          strcpy(mev.chars, "Err"); // error sf

        mev.size = strlen(mev.chars); // change META length
        DUMP(" ", mev.chars);
      }
      break;

      case 0x00:  // Sequence Number
      {
        uint16_t x = readMultiByte(&mf->_fd, MB_WORD);

        mev.data[0] = (x >> 8) & 0xFF;
        mev.data[1] = x & 0xFF;

        DUMP("SEQUENCE NUMBER ", mev.data[0]);
        DUMP(" ", mev.data[1]);
      }
      break;

      case 0x20:  // Channel Prefix
      mev.data[0] = readMultiByte(&mf->_fd, MB_BYTE);
      DUMP("CHANNEL PREFIX ", mev.data[0]);
      break;

      case 0x21:  // Port Prefix
      mev.data[0] = readMultiByte(&mf->_fd, MB_BYTE);
      DUMP("PORT PREFIX ", mev.data[0]);
      break;

#if SHOW_UNUSED_META
      case 0x01:  // Text
      DUMPS("TEXT ");
      for (int i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x02:  // Copyright Notice
      DUMPS("COPYRIGHT ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x03:  // Sequence or Track Name
      DUMPS("SEQ/TRK NAME ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x04:  // Instrument Name
      DUMPS("INSTRUMENT ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x05:  // Lyric
      DUMPS("LYRIC ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x06:  // Marker
      DUMPS("MARKER ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x07:  // Cue Point
      DUMPS("CUE POINT ");
      for (uint8_t i=0; i<mLen; i++)
        DUMP("", (char)mf->_fd.read());
      break;

      case 0x54:  // SMPTE Offset
      DUMPS("SMPTE OFFSET");
      for (uint8_t i=0; i<mLen; i++)
      {
        DUMP(" ", mf->_fd.read());
      }
      break;

      case 0x7F:  // Sequencer Specific Metadata
      DUMPS("SEQ SPECIFIC");
      for (uint8_t i=0; i<mLen; i++)
      {
        DUMPX(" ", mf->_fd.read());
      }
      break;
#endif // SHOW_UNUSED_META

      default:
      {
        uint8_t minLen = min(ARRAY_SIZE(mev.data), mLen);
        
        for (uint8_t i = 0; i < minLen; ++i)
          mev.data[i] = mf->_fd.read(); // read next

        mev.chars[minLen] = '\0'; // in case it is a string
        if (mLen > ARRAY_SIZE(mev.data))
          mf->_fd.seekCur(mLen-ARRAY_SIZE(mev.data));
  //    DUMPS("IGNORED");
      }
      break;
    }
    if (mf->_metaHandler != nullptr)
      (mf->_metaHandler)(&mev);
  }
  break;
  
// ---------------------------- UNKNOWN
  default:
    // stop playing this track as we cannot identify the eType
    _endOfTrack = true;
    DUMPX("[UKNOWN 0x", eType);
    DUMPS("] Track aborted");
    break;
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
    char    h[MTRK_HDR_SIZE+1]; // Header characters + nul
  
    mf->_fd.fgets(h, MTRK_HDR_SIZE+1);
    h[MTRK_HDR_SIZE] = '\0';

    if (strcmp(h, MTRK_HDR) != 0)
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
  DUMP("\n[Track ", _trackId);
  DUMPS(" Header]");
  DUMP("\nLength:\t\t\t", _length);
  DUMP("\nFile Location:\t\t", _startOffset);
  DUMP("\nEnd of Track:\t\t", _endOfTrack);
  DUMP("\nCurrent buffer offset:\t", _currOffset);
}
#endif // DUMP_DATA

