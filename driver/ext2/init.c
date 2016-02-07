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
#include <string.h>
#include <stdio.h>

static int dev_read(uint64_t start, size_t size, void* data, void* prv)
{
    struct cdi_fs_filesystem* fs = (struct cdi_fs_filesystem*) prv;

    return cdi_fs_data_read(fs, start, size, data) == size;
}

static int dev_write(uint64_t start, size_t size, const void* data, void* prv)
{
    struct cdi_fs_filesystem* fs = (struct cdi_fs_filesystem*) prv;

    return cdi_fs_data_write(fs, start, size, data);
}

int ext2_fs_probe(struct cdi_fs_filesystem* cdi_fs, char** volname)
{
    ext2_superblock_t sb;
    int ret;

    ext2_fs_t fs = {
        .dev_read = dev_read,
        .dev_private = cdi_fs,
    };

    ret = ext2_sb_read(&fs, &sb);
    if (ret == 0) {
        return 0;
    }

    *volname = malloc(sizeof(sb.volume_name) + 1);
    memcpy(*volname, sb.volume_name, sizeof(sb.volume_name));
    (*volname)[sizeof(sb.volume_name)] = '\0';

    return 1;
}

int ext2_fs_init(struct cdi_fs_filesystem* cdi_fs)
{
    struct ext2_fs_res* root_res;
    ext2_fs_t* fs = malloc(sizeof(*fs));

    cdi_fs->opaque = fs;
    fs->opaque = cdi_fs;


    fs->dev_read = dev_read;
    fs->dev_write = dev_write;
    fs->dev_private = cdi_fs;

    fs->cache_create = cache_create;
    fs->cache_destroy = cache_destroy;
    fs->cache_sync = cache_sync;
    fs->cache_block = cache_block;
    fs->cache_block_dirty = cache_block_dirty;
    fs->cache_block_free = cache_block_free;

    if (!ext2_fs_mount(fs)) {
        free(fs);
        // XXX Ist das die richtige Fehlernummer?
        cdi_fs->error = CDI_FS_ERROR_ONS;
        return 0;
    }

    root_res = malloc(sizeof(*root_res));
    memset(root_res, 0, sizeof(*root_res));
    root_res->inode_num = EXT2_ROOTDIR_INODE;
    root_res->res.name = strdup("/");
    root_res->res.res = &ext2_fs_res;
    root_res->res.dir = &ext2_fs_dir;

    cdi_fs->root_res = (struct cdi_fs_res*) root_res;
    puts("mounted");
    return 1;
}

int ext2_fs_destroy(struct cdi_fs_filesystem* fs)
{
    ext2_fs_sync(fs->opaque);
    return ext2_fs_unmount(fs->opaque);
}

