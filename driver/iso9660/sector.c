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
#include "cdi/cache.h"

#include "iso9660def.h"

#include "iso9660_cdi.h"

#define MIN(a,b) ((a) > (b) ? (b) : (a))

/**
 * Reads data from device for cache
 *  @param cache CDI cache
 *  @param block Block to read
 *  @param count How many blocks to read
 *  @param dest Buffer to store data in
 *  @param prv Private data (CDI filesystem)
 */
int iso9660_sector_read_cache(struct cdi_cache *cache,uint64_t block,size_t count,void *dest,void *prv) {
  debug("iso9660_sector_read_cache(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n",cache,block,count,dest,prv);
  uint64_t start = ((uint64_t)block)*ISO9660_DEFAULT_SECTOR_SIZE;
  size_t size = count*ISO9660_DEFAULT_SECTOR_SIZE;
  return iso9660_sector_read(prv,start,size,dest)/ISO9660_DEFAULT_SECTOR_SIZE;
}

/**
 * Read data from resource
 *  @param res Resource to read data from
 *  @param pos Position in resource
 *  @param size How many bytes to read
 *  @param Buffer Buffer to store data in
 *  @return How many bytes read
 */
size_t iso9660_read(struct iso9660_fs_res *res,size_t pos,size_t size,void *buffer) {
  size_t block = pos/res->voldesc->sector_size;
  size_t offset = pos%res->voldesc->sector_size;
  size_t rem_size = size;

  while (rem_size>0) {
    //debug("Block: 0x%x\n",res->data_sector+block++);
    struct cdi_cache_block* cache_block;
    size_t cur_size;

    cache_block = cdi_cache_block_get(res->cache, res->data_sector + block, 0);
    if (cache_block == NULL) {
        break;
    }

    cur_size = MIN(rem_size, res->voldesc->sector_size - offset);

    memcpy(buffer,cache_block->data+offset,cur_size);
    cdi_cache_block_release(res->cache,cache_block);

    block++;
    buffer += cur_size;
    rem_size -= cur_size;
    offset = 0;
  }

  return size - rem_size;
}

