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

#ifndef _VOLUME_DESCRIPTOR_H_
#define _VOLUME_DESCRIPTOR_H_

#include <stdint.h>

#include "directory_record.h"

// ISO9660 volume descriptor type
typedef enum {
  ISO9660_VOLDESC_TERM = 255,
  ISO9660_VOLDESC_PRIM = 1
} iso9660_voldesc_type_t;

// ISO9660 volume descriptor header
struct iso9660_voldesc {
  iso9660_voldesc_type_t type:8;
  uint8_t signature[6];
} __attribute__ ((packed));

// Date from ISO9660 volume descriptor
struct i9660_voldesc_date {
  // Year (as ASCII)
  uint32_t year[4];

  // Month (as ASCII; 01=January, 02=February...)
  uint8_t month[2];

  // Day of month (as ASCII)
  uint8_t day[2];

  // Hour (as ASCII)
  uint8_t hour[2];

  // Minute (as ASCII)
  uint8_t minute[2];

  // Second (as ASCII)
  uint8_t second[2];

  // Centisecond (as ASCII; hundredths of a second)
  uint8_t centisecond[2];

  // Offset to GMT (in 15min intervals)
  int8_t gmtoff;
} __attribute__ ((packed));

struct iso9660_voldesc_prim {
  // Volume Descriptor Header
  struct iso9660_voldesc header;

  // Zeros (0)
  uint8_t zero1;

  // System Identifier
  uint8_t system_identifier[32];

  // Volume Identifier
  uint8_t volume_identifier[32];

  // Zeros (0)
  uint8_t zero2[8];

  // Total number of sectors
  uint32_t num_sectors;
  uint32_t num_sectors_be;

  // Zeros (0)
  uint8_t zero3[32];

  // Set size (1)
  uint16_t set_size;
  uint16_t set_size_be;

  // Sequence number (1)
  uint16_t seq_num;
  uint16_t seq_num_be;

  // Sector size (must be 2048)
  uint16_t sector_size;
  uint16_t sector_size_be;

  // Size of path table
  uint32_t pathtable_size;
  uint32_t pathtable_size_be;

  // First sectors of each path table
  uint32_t pathtable1_sector;
  uint32_t pathtable2_sector;
  uint32_t pathtable1_sector_be;
  uint32_t pathtable2_sector_be;

  // Root directory record
  struct iso9660_dirrec root_dir;

  // Volume Set Identifier
  uint8_t volume_set_identifier[128];

  // Publisher Identifier
  uint8_t publisher_identifier[128];

  // Data Preparer Identifier
  uint8_t datapreparer_identifier[128];

  // Application Identifier
  uint8_t application_identifier[128];

  // Copyright File Identifier
  uint8_t copyright_file_identifier[37];

  // Abstract File Identifier
  uint8_t abstract_file_identifier[37];

  // Bibliographical File Identifier
  uint8_t bibliographical_file_identifier[37];

  // Date of volume creation
  struct i9660_voldesc_date date_creation;

  // Date of most recent modification
  struct i9660_voldesc_date date_modification;

  // Date when volume expires
  struct i9660_voldesc_date date_expire;

  // Date when volume is effective
  struct i9660_voldesc_date date_effective;
} __attribute__ ((packed));

int iso9660_voldesc_load(struct cdi_fs_filesystem *fs,iso9660_voldesc_type_t type,void *buf);

#endif
