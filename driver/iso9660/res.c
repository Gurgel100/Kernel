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

#include <string.h>
#include <stdlib.h>

#include "cdi/lists.h"
#include "cdi/fs.h"
#include "cdi/cache.h"

#include "volume_descriptor.h"

#include "iso9660_cdi.h"

/**
 * Creates a new resource
 *  @param name Name of resource
 *  @param parent Parent resource
 *  @param class File class
 *  @param type Special file type
 *  @return ISO9660 class
 */
struct iso9660_fs_res *iso9660_fs_res_create(const char *name,struct iso9660_fs_res *parent,cdi_fs_res_class_t class,cdi_fs_res_type_t type) {
  debug("iso9660_fs_res_create(%s,0x%x,%d)\n",name,parent,class);
  struct iso9660_fs_res *res = malloc(sizeof(struct iso9660_fs_res));

  memset(res,0,sizeof(struct iso9660_fs_res));
  res->res.name = strdup(name);
  res->res.res = &iso9660_fs_res_res;
  res->res.parent = (struct cdi_fs_res*)parent;
  res->class = class;
  res->res.type = type;
  if (class==CDI_FS_CLASS_DIR) res->res.dir = &iso9660_fs_res_dir;
  else res->res.file = &iso9660_fs_res_file;
  res->res.flags.read = 1;
  res->res.flags.execute = 1;
  res->res.flags.browse = 1;
  res->res.flags.read_link = 1;

  if (parent!=NULL) {
    res->voldesc = parent->voldesc;
    res->cache = parent->cache;
  }
  return res;
}

/**
 * Destroys a resource
 *  @param res ISO9660 resource
 *  @return 0=success; -1=failure
 */
int iso9660_fs_res_destroy(struct iso9660_fs_res *res) {
  debug("iso9660_fs_res_destroy(0x%x)\n",res);
  free(res->res.name);
  if (res->res.children!=NULL) {
    size_t i;
    struct iso9660_fs_res *child;
    for (i=0;(child = cdi_list_get(res->res.children,i));i++) iso9660_fs_res_destroy(child);
    cdi_list_destroy(res->res.children);
  }
  return 0;
}

/**
 * Loads a resource
 *  @param stream CDI FS stream
 *  @return 0=success; -1=failure
 */
int iso9660_fs_res_load(struct cdi_fs_stream *stream) {
  struct iso9660_fs_res *res = (struct iso9660_fs_res*)stream->res;

  if (!res->res.loaded) {
    if (res->class==CDI_FS_CLASS_DIR) res->res.children = iso9660_dir_load(res);
    else res->res.children = cdi_list_create();
    res->res.loaded = 1;
  }
  return 1;
}

/**
 * Unloads a resource
 *  @param stream CDI FS stream
 *  @return 0=success; -1=failure
 */
int iso9660_fs_res_unload(struct cdi_fs_stream *stream) {
  struct iso9660_fs_res *res = (struct iso9660_fs_res*)stream->res;

  if (res->res.loaded) {
    // Destroy children
    struct iso9660_fs_res *child;
    while ((child = cdi_list_pop(res->res.children))) iso9660_fs_res_destroy(child);
    cdi_list_destroy(res->res.children);

    res->res.loaded = 0;
  }
  return 1;
}

/**
 * Reads meta data from resource
 *  @param stream CDI FS stream
 *  @param meta Type of meta data
 *  @return Meta data
 */
int64_t iso9660_fs_res_meta_read(struct cdi_fs_stream *stream,cdi_fs_meta_t meta) {
  struct iso9660_fs_res *res = (struct iso9660_fs_res*)stream->res;
  switch (meta) {
    case CDI_FS_META_SIZE:
      return res->res.dir!=NULL?0:res->data_size;

    case CDI_FS_META_USEDBLOCKS:
      return (res->data_size-1)/res->voldesc->sector_size+1;

    case CDI_FS_META_BESTBLOCKSZ:
    case CDI_FS_META_BLOCKSZ:
      return res->voldesc->sector_size;

    case CDI_FS_META_CREATETIME:
      return res->ctime;

    case CDI_FS_META_ACCESSTIME:
      return res->atime;

    case CDI_FS_META_CHANGETIME:
      return res->mtime;
  }
  return 0;
}
