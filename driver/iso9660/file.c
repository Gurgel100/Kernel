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

#include "cdi/fs.h"
#include "cdi/cache.h"

#include "iso9660_cdi.h"

/**
 * Reads from file
 *  @param stream CDI FS stream
 *  @param start Offset in flie
 *  @param size How many bytes to read
 *  @param buffer Buffer to store data in
 *  @return How many bytes read
 */
size_t iso9660_fs_file_read(struct cdi_fs_stream *stream,uint64_t start,size_t size,void *buffer) {
  debug("iso9660_fs_file_read(0x%x,0x%x,0x%x,0x%x,0x%x)\n",stream,start,size,buffer);
  struct iso9660_fs_res *res = (struct iso9660_fs_res*)stream->res;

  if (start>res->data_size) return 0;
  if (start+size>res->data_size) size = res->data_size-start;

  iso9660_read(res,start,size,buffer);
  //cdi_cache_entry_read(res->cache_entry,start,size,buffer);
  return size;
}
