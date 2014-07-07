/*
  MD_MIDIHelper.h - An Arduino library for processing Standard MIDI Files (SMF).
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

#ifndef _MDMIDIHELPER_H
#define	_MDMIDIHELPER_H

// readMultiByte parameter
#define MB_LONG 4
#define	MB_TRYTE 3
#define MB_WORD 2
#define MB_BYTE 1

// function prototypes
uint32_t  readMultiByte(SdFile *f, uint8_t nLen);	// read multi byte value starting with MSB
uint32_t  readVarLen(SdFile *f);         	        // read variable length parameter from input
#if DUMP_DATA
void	  dumpBuffer(uint8_t *p, int len);			// Formatted dump of a buffer of data
#endif // DUMP_DATA

#endif
