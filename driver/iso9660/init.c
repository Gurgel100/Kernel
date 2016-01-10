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

#include <stdlib.h>
#include <stdio.h>

#include "cdi/fs.h"
#include "cdi/cache.h"

#include "iso9660_cdi.h"

#include "volume_descriptor.h"

/**
 * Initializes a ISO9660 filesystem
 *  @param fs Filesystem to initialize
 *  @return If initialization was successful
 */
int iso9660_fs_init(struct cdi_fs_filesystem *fs) {
  debug("iso9660_fs_init(0x%p)\n",fs);
  struct iso9660_voldesc_prim *voldesc = malloc(sizeof(struct iso9660_voldesc_prim));
  if (iso9660_voldesc_load(fs,ISO9660_VOLDESC_PRIM,voldesc)!=-1) {
    struct iso9660_fs_res *root_res = iso9660_dirrec_load(&voldesc->root_dir,NULL,voldesc);
    root_res->cache = cdi_cache_create(voldesc->sector_size,0,iso9660_sector_read_cache,NULL,fs);
    fs->root_res = (struct cdi_fs_res*)root_res;
    return 1;
  }
  else return 0;
}

/**
 * Destroys a FS
 *  @param fs Filesystem to destroy
 *  @return If destroy was successful
 */
int iso9660_fs_destroy(struct cdi_fs_filesystem *fs) {
  fprintf(stderr,"iso9660_fs_destroy(0x%p)\n",fs);
  struct iso9660_fs_res *root_res = (struct iso9660_fs_res*)(fs->root_res);
  free(root_res->voldesc);
  cdi_cache_destroy(root_res->cache);
  return iso9660_fs_res_destroy(root_res);
}
