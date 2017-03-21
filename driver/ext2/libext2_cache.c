/*
 * Copyright (c) 2008 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Antoine Kaufmann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed by the tyndur Project
 *     and its contributors.
 * 4. Neither the name of the tyndur Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ext2_cdi.h"

/** Verbindung zwischen dem CDI-Cache-Eintrag und dem libext2-Cacheblock */
typedef struct cache_block {
    /** Libext2-Cacheblock */
    ext2_cache_block_t      libext2;

    /** Cache-Entry von CDI */
    struct cdi_cache_entry* cdi;
} cache_block_t;


static int read_block(struct cdi_cache* cache, uint64_t block, size_t count,
    void* dest, void* prv_data)
{
    struct ext2_fs* fs = prv_data;
    struct cdi_fs_filesystem* cdi_fs = fs->opaque;
    size_t bs = cache->block_size;

    return cdi_fs_data_read(cdi_fs, bs * block, count * bs, dest) / bs;
}

static int write_block(struct cdi_cache* cache, uint64_t block, size_t count,
    const void* src, void* prv_data)
{
    struct ext2_fs* fs = prv_data;
    struct cdi_fs_filesystem* cdi_fs = fs->opaque;
    size_t bs = cache->block_size;

    return cdi_fs_data_write(cdi_fs, bs * block, count * bs, src) / bs;
}

void* cache_create(struct ext2_fs* fs, size_t block_size)
{
    return cdi_cache_create(block_size, sizeof(ext2_cache_block_t),
        read_block, write_block, fs);
}

void cache_destroy(void* handle)
{
    cdi_cache_destroy(handle);
}

void cache_sync(void* handle)
{
    cdi_cache_sync(handle);
}

ext2_cache_block_t* cache_block(void* handle, uint64_t block, int noread)
{
    struct cdi_cache* c = handle;
    struct cdi_cache_block* cdi_b;
    ext2_cache_block_t* b;

    if (!(cdi_b = cdi_cache_block_get(c, block, noread))) {
        return NULL;
    }

    b = cdi_b->private;
    b->opaque = cdi_b;
    b->data = cdi_b->data;
    b->cache = handle;
    b->number = block;
    return b;
}

void cache_block_dirty(ext2_cache_block_t* b)
{
    struct cdi_cache* c = b->cache;
    struct cdi_cache_block* cdi_b = b->opaque;

    cdi_cache_block_dirty(c, cdi_b);
}

void cache_block_free(ext2_cache_block_t* b, int dirty)
{
    struct cdi_cache* c = b->cache;
    struct cdi_cache_block* cdi_b = b->opaque;

    if (dirty) {
        cache_block_dirty(b);
    }

    cdi_cache_block_release(c, cdi_b);
}

