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

#ifndef _ISO9660_ROCKRIDGE_H_
#define _ISO9660_ROCKRIDGE_H_

#include <sys/types.h>
#include <stdint.h>

#include "directory_record.h"

#define ISO9660_ROCKRIDGE_VERSION 1

#define ISO9660_ROCKRIDGE_SL_FLAG_LAST 1

#define ISO9660_ROCKRIDGE_NAMEFLAG_CONTINUE 1
#define ISO9660_ROCKRIDGE_NAMEFLAG_CURRENT  2
#define ISO9660_ROCKRIDGE_NAMEFLAG_PARENT   4
#define ISO9660_ROCKRIDGE_NAMEFLAG_ROOT     8

#define ISO9660_ROCKRIDGE_TFFLAG_CREATION   1
#define ISO9660_ROCKRIDGE_TFFLAG_MODIFY     2
#define ISO9660_ROCKRIDGE_TFFLAG_ACCESS     4
#define ISO9660_ROCKRIDGE_TFFLAG_ATTRIBUTES 8
#define ISO9660_ROCKRIDGE_TFFLAG_BACKUP     16
#define ISO9660_ROCKRIDGE_TFFLAG_EXPIRATION 32
#define ISO9660_ROCKRIDGE_TFFLAG_EFFECTIVE  64
#define ISO9660_ROCKRIDGE_TFFLAG_LONGFORM   128

#define ISO9660_ROCKRIDGE_SIG_SP ('S'|('P'<<8))
#define ISO9660_ROCKRIDGE_SIG_ST ('S'|('T'<<8))
#define ISO9660_ROCKRIDGE_SIG_RR ('R'|('R'<<8)) ///< @todo Actually the SUE must start with SP and not with RR
#define ISO9660_ROCKRIDGE_SIG_PX ('P'|('X'<<8))
#define ISO9660_ROCKRIDGE_SIG_PN ('P'|('N'<<8))
#define ISO9660_ROCKRIDGE_SIG_SL ('S'|('L'<<8))
#define ISO9660_ROCKRIDGE_SIG_NM ('N'|('M'<<8))
#define ISO9660_ROCKRIDGE_SIG_CL ('C'|('L'<<8))
#define ISO9660_ROCKRIDGE_SIG_PL ('P'|('L'<<8))
#define ISO9660_ROCKRIDGE_SIG_RE ('R'|('E'<<8))
#define ISO9660_ROCKRIDGE_SIG_RF ('R'|('F'<<8))
#define ISO9660_ROCKRIDGE_SIG_SF ('S'|('F'<<8))

// Some POSIX stat stuff

#define RS_IFMT   00170000
#define RS_IFSOCK 00140000
#define RS_IFLNK  00120000
#define RS_IFREG  00100000
#define RS_IFBLK  00060000
#define RS_IFDIR  00040000
#define RS_IFCHR  00020000
#define RS_IFIFO  00010000

#define RS_ISLNK(m)  (((m)&RS_IFMT)==RS_IFLNK)
#define RS_ISREG(m)  (((m)&RS_IFMT)==RS_IFREG)
#define RS_ISDIR(m)  (((m)&RS_IFMT)==RS_IFDIR)
#define RS_ISCHR(m)  (((m)&RS_IFMT)==RS_IFCHR)
#define RS_ISBLK(m)  (((m)&RS_IFMT)==RS_IFBLK)
#define RS_ISFIFO(m) (((m)&RS_IFMT)==RS_IFIFO)
#define RS_ISSOCK(m) (((m)&RS_IFMT)==RS_IFSOCK)

typedef unsigned int rmode_t;

// Rockridge System Use Entry
struct iso9660_rockridge {
  // Field signature
  uint16_t sig;

  // Size of field
  uint8_t size;

  // Version
  uint8_t version;
} __attribute__ ((packed));

/// @todo Actually the SUE must start with SP and not with RR
struct iso9660_rockridge_rr {
  // Size: 5
  struct iso9660_rockridge header;
  uint8_t unknown;
} __attribute__ ((packed));

// Rockridge System Use Entry for file permissions
struct iso9660_rockridge_px {
  // Size: 44
  struct iso9660_rockridge header;

  // File mode (like in sys/stat.h)
  uint32_t mode;
  uint32_t mode_be;

  // Number of links (like in sys/stat.h)
  uint32_t nlink;
  uint32_t nlink_be;

  // Owner UID
  uint32_t uid;
  uint32_t uid_be;

  // Owner GID
  uint32_t gid;
  uint32_t gid_be;

  // File serial number
  uint32_t serial;
  uint32_t serial_be;
} __attribute__ ((packed));

// Rockridge System Use Entry for device nodes
struct iso9660_rockridge_pn {
  // Size: 20
  struct iso9660_rockridge header;

  // Device number
  uint32_t devhi;
  uint32_t devhi_be;
  uint32_t devlo;
  uint32_t devlo_be;
} __attribute__ ((packed));

// Rockridge SL Component
struct iso9660_rockridge_sl_component {
  uint8_t flags;
  uint8_t len;
  uint8_t content[0];
} __attribute__ ((packed));

// Rockridge System Use Entry for symbolic links
struct iso9660_rockridge_sl {
  // Size:
  struct iso9660_rockridge header;

  // Flags
  uint8_t flags;

  // Component Area
  struct iso9660_rockridge_sl_component component_area[0];
} __attribute__ ((packed));

// Rockridge System Use Entry for POSIX names
struct iso9660_rockridge_nm {
  // Size:
  struct iso9660_rockridge header;

  // Flags
  uint8_t flags;

  // Name content
  uint8_t name[1];
} __attribute__ ((packed));

// Rockridge System Use Entry for timestamps
struct iso9660_rockridge_tf {
  // Size:
  struct iso9660_rockridge header;

  // Flags (Type of timestamp
  uint8_t flags;

  uint32_t timestamps[0];
} __attribute__ ((packed));

int iso9660_rockridge_scan(struct iso9660_dirrec *dirrec,char **_name,mode_t *mode,uid_t *uid,gid_t *gid,nlink_t *nlink,uint64_t *atime,uint64_t *ctime,uint64_t *mtime);

#endif
