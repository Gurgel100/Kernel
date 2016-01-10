/*
    iso9660 - An iso9660 CDI driver with Rockridge support
    Copyright (C) 2008  Janosch Gräf <janosch.graef@gmx.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _DIRECTORY_RECORD_H_
#define _DIRECTORY_RECORD_H_

#include <stdint.h>

// Date from ISO9660 directory entry
struct iso9660_dirrec_date {
  // Number of years since 1900
  uint8_t year;

  // Month (1=January, 2=February)
  uint8_t month;

  // Day of month
  uint8_t day;

  // Hour
  uint8_t hour;

  // Minute
  uint8_t minute;

  // Second
  uint8_t second;

  // GMT offset (in 15min intervals)
  int8_t gmtoff;
} __attribute__ ((packed));

#define ISO9660_DIRREC_HIDDEN   1
#define ISO9660_DIRREC_DIR      2
#define ISO9660_DIRREC_ASSOC    4
#define ISO9660_DIRREC_RECFMT   8
#define ISO9660_DIRREC_PERM     16
#define ISO9660_DIRREC_NOTFINAL 128

// ISO9660 directory entry
struct iso9660_dirrec {
  // Size of this record (must be even)
  uint8_t record_size;

  // Number of sectors in extended attribute record
  uint8_t num_sectors_extattrrec;

  // First sector for file/directory data (0 if empty)
  uint32_t data_sector;
  uint32_t data_sector_be;

  // File/directory data size
  uint32_t data_size;
  uint32_t data_size_be;

  // Date of creation
  struct iso9660_dirrec_date date_creation;

  // File flags
  uint8_t flags;

  // Unit size for interleaved files (should be 0)
  uint8_t interleaved_unit_size;

  // Gap size for interleaved files (should be 0)
  uint8_t interleaved_gap_size;

  // Volume sequence number
  uint16_t volume_seq;
  uint16_t volume_seq_be;

  // Identifier length
  uint8_t identifier_length;

  // Identifier
  uint8_t identifier[1];
} __attribute__ ((packed));

#endif
