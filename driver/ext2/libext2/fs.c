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
#include <ext2.h>
#include <stdlib.h>


/**
 * Prueft ob das Dateisystem konsistent aussieht, ohne Bitmap-Checks und co.
 * Dazu muss die Superblockkopie in der FS-Struktur geladen sein.
 *
 * @return 1 wenn das Dateisystem in Ordung ist, 0 sonst.
 */
#if 0
static int fs_check_fast(ext2_fs_t* fs)
{
    int count = ext2_sb_copy_count(fs->sb);
    int i;
    ext2_superblock_t sb;

    // Superblockkopien vergleichen
    for (i = 0; i < count; i++) {
        if (!ext2_sb_read_copy(fs, &sb, i)) {
            return 0;
        }

        if (!ext2_sb_compare(fs->sb, &sb)) {
            return 0;
        }
    }
    return 1;
}
#endif

int ext2_fs_mount(ext2_fs_t* fs)
{
    fs->sb = malloc(sizeof(ext2_superblock_t));
    fs->boot_sectors = malloc(1024);

    if (!fs->sb || !fs->boot_sectors) {
        goto fail;
    }

    if (!ext2_sb_read(fs, fs->sb)) {
        goto fail;
    }

    if (!fs->dev_read(0, 1024, fs->boot_sectors, fs->dev_private)) {
        goto fail;
    }

    fs->cache_handle = fs->cache_create(fs, ext2_sb_blocksize(fs->sb));
    fs->block_prev_alloc = 0;

    // Die Blocknummer in der der Superblock liegt, variiert je nach
    // Blockgroesse, da der offset fix 1024 ist.
    fs->sb_block = (ext2_sb_blocksize(fs->sb) > 1024 ? 0 : 1);

#if 0
    if (!fs_check_fast(fs)) {
        free(fs->sb);
        fs->sb = NULL;
        return 0;
    }
#endif
    return 1;

fail:
    free(fs->boot_sectors);
    free(fs->sb);
    fs->sb = NULL;
    return 0;
}

int ext2_fs_unmount(ext2_fs_t* fs)
{
    // Sicherstellen, dass alle Aenderungen am Superblock geschrieben sind
    if (!ext2_sb_update(fs, fs->sb)) {
        return 0;
    }

    free(fs->boot_sectors);
    free(fs->sb);
    fs->cache_destroy(fs->cache_handle);
    fs->sb = fs->cache_handle = NULL;
    return 1;
}

void ext2_fs_sync(ext2_fs_t* fs)
{
    fs->cache_sync(fs->cache_handle);
}
