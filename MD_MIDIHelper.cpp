/*
  MD_MIDIHelper.cpp - An Arduino library for processing Standard MIDI Files (SMF).
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
#include <MD_MIDIFile.h>
#include "MD_MIDIHelper.h"

uint32_t readMultiByte(SdFile *f, uint8_t nLen)
// read a multi byte value with MSB from the input stream
{
  uint32_t  value = 0L;
  
  for (uint8_t i=0; i<nLen; i++)
  {
    value = (value << 8) + f->read();
  }
  
  return(value);
}

uint32_t readVarLen(SdFile *f)
// read a variable length parameter from the input stream
{
  uint32_t  value = 0;
  char      c;
  
  do
  {
    c = f->read();
    value = (value << 7) + (c & 0x7f);
  }  while (c & 0x80);
  
  return(value);
}

/*
// this is the old code proposed by the MIDI organisation
// new code is probably more efficient

uint32_t readVarLen(SdFile *f)
// read a variable length parameter from the input stream
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
  
  return(value);
}
*/

#if DUMP_DATA
void dumpBuffer(uint8_t *p, int len)
{
  for (int i=0; i<len; i++, p++)
  {
    if ((i!=0) && ((i & 0x0f) == 0)) // every 16 characters
		DUMP('\n');
    DUMP(' ');
    if (*p<=0xf) 
		DUMP('0');
    DUMPX(*p);
  }
}
#endif
