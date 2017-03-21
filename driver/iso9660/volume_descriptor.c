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

#include <stdint.h>
#include <string.h>

#include "cdi/fs.h"

#include "iso9660_cdi.h"

#include "iso9660def.h"
#include "volume_descriptor.h"

/**
 * Checks signature of volume descriptor
 *  @param sig Signature
 *  @return If signature is right
 */
static int iso9660_voldesc_checksig(uint8_t sig[6]) {
  return 1;
  //char right[6] = {'C','D','0','0','1',1};
  //return memcmp(right,sig,6)==0;
}

/**
 * Loads a volume descriptor
 *  @param fs Filesystem
 *  @param type Type of volume descriptor
 *  @param buf Buffer for volume descriptor
 *  @return 0=success; -1=failure
 */
int iso9660_voldesc_load(struct cdi_fs_filesystem *fs,iso9660_voldesc_type_t type,void *buf) {
  debug("iso9660_voldesc_load(0x%x,%d,0x%x)\n",fs,type,buf);
  struct iso9660_voldesc header;
  size_t i;

  for (i=ISO9660_FIRST_SECTOR;header.type!=type && header.type!=ISO9660_VOLDESC_TERM;i++) {
    iso9660_sector_read(fs,i*ISO9660_DEFAULT_SECTOR_SIZE,sizeof(header),&header);
    if (!iso9660_voldesc_checksig(header.signature)) return -1;
  }
  i--;

  if (header.type==type) {
    iso9660_sector_read(fs,i*ISO9660_DEFAULT_SECTOR_SIZE,sizeof(struct iso9660_voldesc_prim),buf);
    return 0;
  }
  else {
    debug("Volume Descriptor: Wrong type: %d\n",header.type);
    return -1;
  }
}
