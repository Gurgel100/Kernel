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

#ifndef _ISO9660_CDI_H_
#define _ISO9660_CDI_H_

#include <stdint.h>
#include <stddef.h>

#include "cdi/fs.h"
#include "cdi/cache.h"
#include "cdi/lists.h"

#include "iso9660def.h"
#include "volume_descriptor.h"

// ISO9660 Resource
struct iso9660_fs_res {
  struct cdi_fs_res res;

  // Creation time
  uint64_t ctime;

  // Access time
  uint64_t atime;

  // Modification time
  uint64_t mtime;

  // First sector with resource data
  uint64_t data_sector;

  // Size of resource data
  uint32_t data_size;

  // File class
  cdi_fs_res_class_t class;

  // Primary volume descriptor
  struct iso9660_voldesc_prim *voldesc;

  // Cache
  struct cdi_cache *cache;
};

// init.c
int iso9660_fs_init(struct cdi_fs_filesystem *fs);
int iso9660_fs_destroy(struct cdi_fs_filesystem *fs);

// res.c
struct iso9660_fs_res *iso9660_fs_res_create(const char *name,struct iso9660_fs_res *parent,cdi_fs_res_class_t class,cdi_fs_res_type_t type);
int iso9660_fs_res_destroy(struct iso9660_fs_res *res);
int iso9660_fs_res_load(struct cdi_fs_stream *stream);
int iso9660_fs_res_unload(struct cdi_fs_stream *stream);
int64_t iso9660_fs_res_meta_read(struct cdi_fs_stream *stream,cdi_fs_meta_t meta);

// file.c
size_t iso9660_fs_file_read(struct cdi_fs_stream *stream,uint64_t start,size_t size,void *buffer);

// dir.c
cdi_list_t iso9660_dir_load(struct iso9660_fs_res *res);
struct iso9660_fs_res *iso9660_dirrec_load(struct iso9660_dirrec *dirrec,struct iso9660_fs_res *parent,struct iso9660_voldesc_prim *voldesc);
cdi_list_t iso9660_fs_dir_list(struct cdi_fs_stream *stream);

// sector.c
#define iso9660_sector_read(fs,start,size,buffer) cdi_fs_data_read(fs,start,size,buffer)
size_t iso9660_read(struct iso9660_fs_res *res,size_t pos,size_t size,void *buffer);
int iso9660_sector_read_cache(struct cdi_cache *cache,uint64_t block,size_t count,void *dest,void *prv);

// resources.c
extern struct cdi_fs_res_res    iso9660_fs_res_res;
extern struct cdi_fs_res_file   iso9660_fs_res_file;
extern struct cdi_fs_res_dir    iso9660_fs_res_dir;

// Debug
int debug(const char *fmt,...);

#endif
