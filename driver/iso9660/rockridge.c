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

#include "iso9660def.h"
#ifdef ISO9660_USE_ROCKRIDGE

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "cdi/lists.h"

#include "rockridge.h"
#include "directory_record.h"

/**
 * Scans system use area in directory record for rockridge entries
 *  @param dirrec Directory record
 *  @param _name Reference for POSIX name
 *  @param mode Reference for file mode
 *  @param uid Reference for UID
 *  @param gid Reference for GID
 *  @param nlink Reference for number of hardlinks
 *  @param atime Reference for access time
 *  @param ctime Reference for createtion time
 *  @param mtime Reference for modification time
 *  @return 0=success; -1=failure
 */
int iso9660_rockridge_scan(struct iso9660_dirrec *dirrec,char **_name,mode_t *mode,uid_t *uid,gid_t *gid,nlink_t *nlink,uint64_t *atime,uint64_t *ctime,uint64_t *mtime) {
  struct iso9660_rockridge *sue = ((void*)(dirrec->identifier))+dirrec->identifier_length;
  if ((((uintptr_t)sue)&1)==1) sue = ((void*)sue)+1;

  if (sue->sig!=ISO9660_ROCKRIDGE_SIG_RR) return -1;

  char *name = NULL;
  size_t name_size = 0;

  while (sue->sig!=ISO9660_ROCKRIDGE_SIG_ST && sue->sig!=0 && ((uintptr_t)sue-(uintptr_t)dirrec)+4<dirrec->record_size) {
    if (sue->sig==ISO9660_ROCKRIDGE_SIG_PX) {
      struct iso9660_rockridge_px *sue_px = (struct iso9660_rockridge_px*)sue;
      if (mode!=NULL) *mode = sue_px->mode;
      if (nlink!=NULL) *nlink = sue_px->nlink;
      if (uid!=NULL) *uid = sue_px->uid;
      if (gid!=NULL) *gid = sue_px->gid;
    }
    else if (sue->sig==ISO9660_ROCKRIDGE_SIG_NM) {
      struct iso9660_rockridge_nm *sue_nm = (struct iso9660_rockridge_nm*)sue;
      if (sue_nm->flags&ISO9660_ROCKRIDGE_NAMEFLAG_CURRENT) *_name = strdup(".");
      else if (sue_nm->flags&ISO9660_ROCKRIDGE_NAMEFLAG_PARENT) *_name = strdup("..");
      else {
        size_t part_size = sue->size-offsetof(struct iso9660_rockridge_nm,name);
        name = realloc(name,name_size+part_size+1);
        memcpy(name+name_size,sue_nm->name,part_size);
        name_size += part_size;
      }
    }
    sue = ((void*)sue)+sue->size;
  }

  if (name!=NULL) {
    if (_name!=NULL) {
      name[name_size] = 0;
      *_name = name;
    }
    else free(name);
  }

  return 0;
}

#endif
