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
#define _MDMIDIHELPER_H

/**
 * \file
 * \brief Header file for helper functions definitions
 */

// Miscellaneous
#define MTHD_HDR      "MThd"    ///< SMF marker
#define MTHD_HDR_SIZE 4         ///< SMF marker length
#define MTRK_HDR      "MTrk"    ///< SMF track header marker
#define MTRK_HDR_SIZE 4         ///< SMF track header marker length

#define BUF_SIZE(x)   (sizeof(x)/sizeof(x[0]))  ///< Buffer size macro

// readMultiByte() parameters
#define MB_LONG 4   ///< readMultibyte() parameter specifying expected 4 byte value
#define	MB_TRYTE 3  ///< readMultibyte() parameter specifying expected 3 byte value
#define MB_WORD 2   ///< readMultibyte() parameter specifying expected 2 byte value
#define MB_BYTE 1   ///< readMultibyte() parameter specifying expected 1 byte value

// Function prototypes ----------------
/**
 * Read a multi byte value from the input stream
 *
 * SMF contain numbers that are fixed length. This function reads these from the input file.
 * 
 * \param *f    pointer to SDFile object to use for reading.
 * \param nLen  one of MB_LONG, MB_TRYTE, MB_WORD, MB_BYTE to specify the number of bytes to read.
 * \return the value read as a 4 byte integer. This should be cast to the expected size if required.
 */
uint32_t readMultiByte(SdFile *f, uint8_t nLen);

/**
 * Read a variable length parameter from the input stream
 *
 * SMF contain numbers that are variable length, with the last byte of the number identified with bit 7 set.
 * This function reads these from the input file.
 *
 * \param *f    pointer to SDFile object to use for reading.
 * \return the value read as a 4 byte integer. This should be cast to the expected size if required.
 */
uint32_t readVarLen(SdFile *f);   

/** 
 * Dump a block of data stream
 *
 * During debugging, this method provides a formatted dump of data to the debug
 * output stream.
 *
 * The DUMP_DATA macro define must be set to 1 to enable this method.
 *
 * \param p   pointer to the data buffer.
 * \param len size of the data buffer.
 */
void dumpBuffer(uint8_t *p, int len);

#endif
