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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cdi/fs.h"
#include "cdi/lists.h"
#include "cdi/cache.h"
#include "cdi/misc.h"

#include "directory_record.h"
#include "volume_descriptor.h"
#include "iso9660def.h"

#ifdef ISO9660_USE_ROCKRIDGE
  #include "rockridge.h"
#endif

#include "iso9660_cdi.h"

#ifdef ISO9660_LOWER_FILENAMES
  #include <ctype.h>
#endif

/**
 * Convert ISO9660 identifier to file name
 *  @param ident Identifier
 *  @param len Length of identifier
 *  @return Name (should be passed to free())
 */
static char *parse_name(char *ident,size_t len) {
  if (ident[len-2]==';') len -= 2;
  if (ident[len-1]=='.') len--;
  char *name = memcpy(malloc(len+1),ident,len);
#ifdef ISO9660_LOWER_FILENAMES
  size_t i;
  for (i=0;i<len;i++) name[i] = tolower(name[i]);
#endif
  name[len] = 0;
  return name;
}

/**
 * Converts POSIX file mode to CDI class
 *  @param mode POSIX filemode
 *  @param special Reference for CDI special type
 *  @return CDI class
 */
#ifdef ISO9660_USE_ROCKRIDGE
static cdi_fs_res_class_t parse_class(rmode_t mode,cdi_fs_res_type_t *special) {
  cdi_fs_res_class_t class = CDI_FS_CLASS_FILE;
  if (RS_ISBLK(mode)) {
    class = CDI_FS_CLASS_SPECIAL;
    if (special!=NULL) *special = CDI_FS_BLOCK;
  }
  else if (RS_ISCHR(mode)) {
    class = CDI_FS_CLASS_SPECIAL;
    if (special!=NULL) *special = CDI_FS_BLOCK;
  }
  else if (RS_ISFIFO(mode)) {
    class = CDI_FS_CLASS_SPECIAL;
    if (special!=NULL) *special = CDI_FS_FIFO;
  }
  else if (RS_ISSOCK(mode)) {
    class = CDI_FS_CLASS_SPECIAL;
    if (special!=NULL) *special = CDI_FS_SOCKET;
  }
  else if (RS_ISREG(mode)) class = CDI_FS_CLASS_FILE;
  else if (RS_ISDIR(mode)) class = CDI_FS_CLASS_DIR;
  else if (RS_ISLNK(mode)) class = CDI_FS_CLASS_LINK;
  return class;
}
#endif

/**
 * Converts ISO9660 time to CDI time
 *  @param date ISO9660-Zeit
 *  @return CDI time
 */
static inline uint64_t parse_date(struct iso9660_dirrec_date *date) {
  // FIXME
  // return cdi_time_by_date(date->year,date->month-1,date->day)+cdi_time_offset(date->hour,date->minute,date->second);
  return 0;
}

/**
 * Loads a ISO9660 directory entry
 *  @param dirrec Directory entry
 *  @param parent Parent resource
 *  @param voldesc Volume descriptor
 *  @return Resource
 */
struct iso9660_fs_res *iso9660_dirrec_load(struct iso9660_dirrec *dirrec,struct iso9660_fs_res *parent,struct iso9660_voldesc_prim *voldesc) {
  char *name = NULL;
  uint64_t ctime = 0;
  uint64_t atime = 0;
  uint64_t mtime = 0;
  cdi_fs_res_class_t class = 0;
  cdi_fs_res_type_t type = 0;

#ifdef ISO9660_USE_ROCKRIDGE
  rmode_t mode;
  if (iso9660_rockridge_scan(dirrec,&name,&mode,NULL,NULL,NULL,&atime,&ctime,&mtime)==0) {
    class = parse_class(mode,&type);
  }
  else {
    name = NULL;
    class = 0;
#endif
    ctime = parse_date(&(dirrec->date_creation));
    atime = ctime;
    mtime = ctime;
#ifdef ISO9660_USE_ROCKRIDGE
  }
#endif
  if (name==NULL) name = parse_name((char*)dirrec->identifier,dirrec->identifier_length);
  if (class==0) class = (dirrec->flags&ISO9660_DIRREC_DIR)?CDI_FS_CLASS_DIR:CDI_FS_CLASS_FILE;

  struct iso9660_fs_res *new = iso9660_fs_res_create(name,parent,class,type);
  if (voldesc!=NULL) new->voldesc = voldesc;
  new->ctime = ctime;
  new->atime = atime;
  new->mtime = mtime;
  new->data_sector = dirrec->data_sector;
  new->data_size = dirrec->data_size;

  free(name);
  return new;
}

/**
 * Loads a directory
 *  @param res Resource to load as directory
 *  @return Directory list
 */
cdi_list_t iso9660_dir_load(struct iso9660_fs_res *res) {
  debug("iso9660_dir_load(0x%x(%s))\n",res,res->res.name);
  struct iso9660_dirrec *dirrec;
  size_t i = 0;
  cdi_list_t dirlist = cdi_list_create();
  size_t curpos = 0;

  while (curpos<res->data_size) {
    uint8_t size;

    iso9660_read(res,curpos,1,&size);
    if (size==0) curpos++;
    else {
      dirrec = malloc(size);
      iso9660_read(res,curpos,size,dirrec);
      if (i>1) cdi_list_push(dirlist,iso9660_dirrec_load(dirrec,res,NULL));

      i++;
      curpos += size;
    }
  }

  return dirlist;
}

/**
 * CDI FS Call to read a directory
 *  @param stream CDI stream
 *  @return Directory list
 */
cdi_list_t iso9660_fs_dir_list(struct cdi_fs_stream *stream) {
  struct iso9660_fs_res *res = (struct iso9660_fs_res*)stream->res;

  return res->res.children;
}
