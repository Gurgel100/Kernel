/*
 * Copyright (c) 2008 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Kevin Wolf.
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

#include <string.h>
#include <stdio.h>
#include "ext2.h"

static uint64_t block_free(ext2_fs_t* fs, uint64_t num);

static inline ext2_cache_block_t* get_bg_block(ext2_fs_t* fs, int group_nr)
{
    uint64_t num;
    size_t bs = ext2_sb_blocksize(fs->sb);

    num = fs->sb_block + 1 + (group_nr * sizeof(ext2_blockgroup_t)) / bs; 
    return fs->cache_block(fs->cache_handle, num, 0);
}

int ext2_bg_read(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg)
{
    ext2_cache_block_t* b = get_bg_block(fs, group_nr);

    if (!b) {
        return 0;
    }

    memcpy(bg, b->data + (group_nr * sizeof(ext2_blockgroup_t)) %
        ext2_sb_blocksize(fs->sb), sizeof(ext2_blockgroup_t));
    fs->cache_block_free(b, 0);
    return 1;
}

int ext2_bg_update(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg)
{
    ext2_cache_block_t* b = get_bg_block(fs, group_nr);

    if (!b) {
        return 0;
    }

    memcpy(b->data + (group_nr * sizeof(ext2_blockgroup_t)) %
        ext2_sb_blocksize(fs->sb), bg, sizeof(ext2_blockgroup_t));
    fs->cache_block_free(b, 1);
    return 1;
}

/**
 * Pointer auf den Cache-Block in dem sich der Inode befindet holen.
 *
 * @param fs        Dateisystem zu dem der Inode gehoert
 * @param inode_nr  Inodenummer
 * @param offset    Wenn != 0 wird an dieser Speicherstelle der Offset vom
 *                  Anfang dieses Blocks aus zum Inode abgelegt.
 *
 * @return Pointer auf das Handle fuer diesen Block
 */
static inline ext2_cache_block_t* inode_get_block(
    ext2_fs_t* fs, uint64_t inode_nr, size_t* offset)
{
    uint64_t    inode_offset;
    uint64_t    inode_internal = ext2_inode_to_internal(fs, inode_nr);
    size_t      inode_size = ext2_sb_inodesize(fs->sb);
    size_t      bs = ext2_sb_blocksize(fs->sb);
    ext2_blockgroup_t bg;

    ext2_bg_read(fs, inode_internal / fs->sb->inodes_per_group, &bg);

    // Offset des Inodes zum Anfang der Tabelle
    inode_offset = inode_size * (inode_internal % fs->sb->inodes_per_group);

    // Offset zum Anfang des Blocks
    if (offset) {
        *offset = inode_offset % bs;
    }

    return fs->cache_block(fs->cache_handle, bg.inode_table +
        inode_offset / bs, 0);
}

int ext2_inode_read(ext2_fs_t* fs, uint64_t inode_nr, ext2_inode_t* inode)
{
    size_t offset;
    ext2_cache_block_t* b;

    if (!(b = inode_get_block(fs, inode_nr, &offset))) {
        return 0;
    }

    inode->raw = b->data + offset;
    inode->block = b;

    inode->fs = fs;
    inode->number = inode_nr;
    return 1;
}

void ext2_inode_release(ext2_inode_t* inode)
{
    ext2_fs_t* fs = inode->fs;
    fs->cache_block_free(inode->block, 0);
    inode->block = NULL;
    inode->raw = NULL;
}

int ext2_inode_update(ext2_inode_t* inode)
{
    ext2_fs_t* fs = inode->fs;
    fs->cache_block_dirty(inode->block);
    return 1;
}

/**
 * Block mit der Inode-Bitmap holen
 *
 * @param fs    Das Dateisystem
 * @param bg    Blockgruppendeskriptor
 *
 * @return Blockhandle
 */
static inline ext2_cache_block_t* ibitmap_get_block(
    ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
    return fs->cache_block(fs->cache_handle, bg->inode_bitmap, 0);
}

/**
 * Neuen Inode allozieren
 *
 * @return Interne Inodenummer
 */
static uint64_t inode_alloc(ext2_fs_t* fs, uint32_t bgnum)
{
    uint32_t* bitmap;
    ext2_cache_block_t* block;
    uint32_t i, j;

    ext2_blockgroup_t bg;
    ext2_bg_read(fs, bgnum, &bg);

    // Bitmap laden
    block = ibitmap_get_block(fs, &bg);
    bitmap = block->data;

    // Freies Bit suchen
    for (i = 0; i < (fs->sb->inodes_per_group + 31) / 32; i++) {
        if (bitmap[i] != ~0U) {
            for (j = 0; j < 32; j++) {
                if (((bitmap[i] & (1 << j)) == 0)
                    && (i * 32 + j <= fs->sb->inodes_per_group))
                {
                    goto found;
                }
            }
        }
    }

    fs->cache_block_free(block, 0);
    return 0;

found:

    // Als besetzt markieren
    bitmap[i] |= (1 << j);
    fs->cache_block_free(block, 1);

    fs->sb->free_inodes--;
    ext2_sb_update(fs, fs->sb);

    bg.free_inodes--;
    ext2_bg_update(fs, bgnum, &bg);

    return (bgnum * fs->sb->inodes_per_group) + (i * 32) + j;
}

int ext2_inode_alloc(ext2_fs_t* fs, ext2_inode_t* inode)
{
    uint64_t number;
    uint32_t bgnum;

    for (bgnum = 0; bgnum < ext2_sb_bgcount(fs->sb); bgnum++) {
        number = inode_alloc(fs, bgnum);
        if (number) {
            goto found;
        }
    }
    return 0;

found:

    if (!number) {
        return 0;
    }

    if (!ext2_inode_read(fs, ext2_inode_from_internal(fs, number), inode)) {
        return 0;
    }

    memset(inode->raw, 0, sizeof(*inode->raw));
    return 1;
}

int ext2_inode_free(ext2_inode_t* inode)
{
    ext2_fs_t* fs = inode->fs;
    uint64_t inode_int = ext2_inode_to_internal(fs, inode->number);
    int i;

    // dtime muss fuer geloeschte Inodes != 0 sein
    inode->raw->deletion_time = inode->fs->sb->inode_count; // FIXME

    // Inodebitmap anpassen
    uint32_t bgnum = inode_int / fs->sb->inodes_per_group;
    uint32_t num = inode_int % fs->sb->inodes_per_group;
    uint32_t* bitmap;
    size_t   block_size = ext2_sb_blocksize(fs->sb);
    ext2_cache_block_t* block;
    ext2_blockgroup_t bg;
    ext2_bg_read(fs, bgnum, &bg);

    block = ibitmap_get_block(fs, &bg);
    bitmap = block->data;

    bitmap[num / 32] &= ~(1 << (num % 32));

    fs->cache_block_free(block, 1);

    fs->sb->free_inodes++;
    ext2_sb_update(fs, fs->sb);

    bg.free_inodes++;
    ext2_bg_update(fs, bgnum, &bg);

    void free_indirect_blocks(uint64_t table_block, int level)
    {
        uint32_t i;
        uint32_t* table;
        ext2_cache_block_t* block;

        block = fs->cache_block(fs->cache_handle, table_block, 0);
        table = block->data;

        for (i = 0; i < block_size / 4; i++) {
            if (table[i]) {
                if (level) {
                    free_indirect_blocks(table[i], level - 1);
                }
                block_free(inode->fs, table[i]);
            }
        }

        fs->cache_block_free(block, 1);
    }

    // Abbrechen bei fast-Symlinks
    if (!inode->raw->block_count) {
        return 1;
    }

    // Blocks freigeben
    for (i = 0; i < 15; i++) {
        if (inode->raw->blocks[i]) {
            if (i >= 12) {
                free_indirect_blocks(inode->raw->blocks[i], i - 12);
            }
            block_free(inode->fs, inode->raw->blocks[i]);
        }
    }

    return 1;
}

static int block_alloc_bg(ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
    uint32_t i;

    i = fs->block_prev_alloc /fs->sb->blocks_per_group;
    if (i < ext2_sb_bgcount(fs->sb)) {
        ext2_bg_read(fs, i, bg);
        if (bg->free_blocks) {
            return i;
        }
    }

    for (i = 0; i < ext2_sb_bgcount(fs->sb); i++) {
        ext2_bg_read(fs, i, bg);
        if (bg->free_blocks) {
            return i;
        }
    }

    return -1;
}

/**
 * Block mit der Block-Bitmap holen
 *
 * @param fs    Das Dateisystem
 * @param bg    Blockgruppendeskriptor
 *
 * @return Blockhandle
 */
static inline ext2_cache_block_t* bbitmap_get_block(
    ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
    return fs->cache_block(fs->cache_handle, bg->block_bitmap, 0);
}


static uint64_t block_alloc(ext2_fs_t* fs, int set_zero)
{
    uint32_t* bitmap;
    uint64_t block_num;
    uint32_t i, j;
    size_t   block_size = ext2_sb_blocksize(fs->sb);
    ext2_cache_block_t* block;
    ext2_cache_block_t* new_block;
    uint32_t bgnum;
    ext2_blockgroup_t bg;

    bgnum = block_alloc_bg(fs, &bg);
    if (bgnum == (uint32_t) -1) {
        return 0;
    }

    // Bitmap laden
    block = bbitmap_get_block(fs, &bg);
    bitmap = block->data;

    // Zuerst Blocks nach dem vorherigen alloziierten pruefen
    if (fs->block_prev_alloc &&
        (fs->block_prev_alloc /fs->sb->blocks_per_group == bgnum))
    {
        uint32_t first_j = fs->block_prev_alloc % 32;
        uint32_t first_i = fs->block_prev_alloc / 32;
        for (i = first_i;i < fs->sb->blocks_per_group / 32; i++) {
            if (bitmap[i] != ~0U) {
                j = (i == first_i) ? first_j : 0;
                for (; j < 32; j++) {
                    if ((bitmap[i] & (1 << j)) == 0) {
                        goto found;
                    }
                }
            }
        }
    }

    // Freies Bit suchen
    for (i = 0; i < fs->sb->blocks_per_group / 32; i++) {
        if (bitmap[i] != ~0U) {
            for (j = 0; j < 32; j++) {
                if ((bitmap[i] & (1 << j)) == 0) {
                    goto found;
                }
            }
        }
    }

    fs->cache_block_free(block, 0);
    return 0;

found:
    block_num = fs->sb->blocks_per_group * bgnum + (i * 32 + j) +
        fs->sb->first_data_block;

    // Mit Nullen initialisieren, falls gewuenscht
    if (set_zero) {
        new_block = fs->cache_block(fs->cache_handle, block_num, 1);
        memset(new_block->data, 0, block_size);
        fs->cache_block_free(new_block, 1);
    }

    // Als besetzt markieren
    bitmap[i] |= (1 << j);
    fs->cache_block_free(block, 1);

    fs->sb->free_blocks--;
    ext2_sb_update(fs, fs->sb);

    bg.free_blocks--;
    ext2_bg_update(fs, bgnum, &bg);

    fs->block_prev_alloc = block_num;
    return block_num;
}

static uint64_t block_free(ext2_fs_t* fs, uint64_t num)
{
    uint32_t* bitmap;
    uint32_t bgnum;
    ext2_cache_block_t* block;

    num -= fs->sb->first_data_block;
    bgnum = num / fs->sb->blocks_per_group;
    num = num % fs->sb->blocks_per_group;

    ext2_blockgroup_t bg;
    ext2_bg_read(fs, bgnum, &bg);

    // Bitmap laden
    block = bbitmap_get_block(fs, &bg);
    bitmap = block->data;

    // Als frei markieren
    bitmap[num / 32] &= ~(1 << (num % 32));
    fs->cache_block_free(block, 1);

    fs->sb->free_blocks++;
    ext2_sb_update(fs, fs->sb);

    bg.free_blocks++;
    ext2_bg_update(fs, bgnum, &bg);

    return 1;
}

/**
 * Berechnet wie oft indirekt dieser Block ist im Inode.
 *
 * @param inode          Betroffener Inode
 * @param block          Blocknummer
 * @param direct_block   Pointer auf die Variable fuer den Index in der
 *                       Blocktabelle des Inodes.
 * @param indirect_block Pointer auf die Variable fuer den Index in den
 *                       indirekten Blocktabellen. Wird auf 0 gesetzt fuer
 *                       direkte Blocks.
 *
 * @return Ein Wert von 0-3 je nach dem wie oft der Block indirekt ist
 */
static inline int get_indirect_block_level(ext2_inode_t* inode, uint64_t block,
    uint64_t* direct_block, uint64_t* indirect_block)
{
    ext2_fs_t* fs = inode->fs;
    size_t block_size = ext2_sb_blocksize(fs->sb);

    if (block < 12) {
        *direct_block = block;
        *indirect_block = 0;
        return 0;
    } else if (block < ((block_size / 4) + 12)) {
        *direct_block = 12;
        *indirect_block = block - 12;
        return 1;
    } else if (block < ((block_size / 4) * ((block_size / 4) + 1) + 12)) {
        *direct_block = 13;
        *indirect_block = block - 12 - (block_size / 4);
        return 2;
    } else {
        *direct_block = 14;
        *indirect_block = block - 12 - (block_size / 4) *
            ((block_size / 4) + 1);
        return 3;
    }
}

static uint64_t get_block_offset(
    ext2_inode_t* inode, uint64_t block, int alloc)
{
    size_t    block_size;
    uint64_t  block_nr;
    uint64_t  indirect_block;
    uint64_t  direct_block;
    int       level;
    int       i;
    uint32_t  j;
    uint32_t* table;
    ext2_cache_block_t* b;
    // Wird zum freigeben von Blocks benoetigt
    uint64_t path[4][2];
    memset(path, 0, sizeof(path));

    ext2_fs_t* fs = inode->fs;
    block_size = ext2_sb_blocksize(fs->sb);


    // Herausfinden wie oft indirekt der gesuchte Block ist
    level = get_indirect_block_level(inode, block, &direct_block,
        &indirect_block);

    // Direkte Blocknummer oder Nummer des ersten Tabellenblocks aus dem Inode
    // lesen
    block_nr = inode->raw->blocks[direct_block];
    if (!block_nr && (alloc == 1)) {
        block_nr = inode->raw->blocks[direct_block] = block_alloc(fs, level);
        inode->raw->block_count += block_size / 512;
    }
    path[0][0] = block_nr;

    // Indirekte Blocks heraussuchen und ggf. allozieren
    for (i = 0;  (i < level) && block_nr; i++) {
        int table_modified;
        uint64_t offset;
        uint64_t pow;

        b = fs->cache_block(fs->cache_handle, block_nr, 0);
        table = b->data;

        // pow = (block_size / 4) ** (level - i - 1)
        pow = (level - i == 3 ? ((block_size / 4) * (block_size / 4)) :
            (level - i == 2 ? (block_size / 4) : 1));
        offset = indirect_block / pow;
        indirect_block %= pow;


        table_modified = 0;
        block_nr = table[offset];

        // Pfad fuers Freigeben der Blocks speichern
        path[i][1] = offset;
        path[i + 1][0] = block_nr;

        // Block allozieren wenn noetig und gewuenscht
        if (!block_nr && (alloc == 1)) {
            block_nr = table[offset] = block_alloc(fs, (level - i) > 1);
            inode->raw->block_count += block_size / 512;
            table_modified = 1;
        }
        fs->cache_block_free(b, table_modified);
    }

    // Wenn der Block nicht freigegeben werden soll, sind wir jetzt fertig
    if (alloc != 2) {
        return block_size * block_nr;
    }

    // Blocks freigeben
    int empty = 1;
    for (i = level; (i > 0) && empty; i--) {
        int was_empty = empty;

        b = fs->cache_block(fs->cache_handle, path[i - 1][0], 0);
        table = b->data;

        // Wenn es sich um einen Tabellenblock handelt, muss er leer sein
        // (Hier wird eigentlich der naechste gepfrueft... das funktioniert aber
        // so, da wir nur in die Schleife kommen wenn es sich um einen
        // indirekten Block handelt, und da der oberste Block bei i = level
        // nicht geprueft werden muss, da er nur ein Datenblock ist.
        for (j = 0; (j < block_size / 4) && empty; j++) {
            if (table[j]) {
                empty = 0;
            }
        }

        if (was_empty && path[i][0]) {
            table[path[i - 1][1]] = 0;
            block_free(fs, path[i][0]);
            inode->raw->block_count -= block_size / 512;
        }

        fs->cache_block_free(b, was_empty);
    }

    // Der Letzte muss manuell freigegeben werden, da er nicht in einer
    // Blocktabelle eingetragen ist.
    if (!path[0][1] && path[0][0] && empty) {
        inode->raw->blocks[direct_block] = 0;

        inode->raw->block_count -= block_size / 512;
        block_free(fs, path[0][0]);
    }
    return block_size * block_nr;
}

int ext2_inode_readblk(ext2_inode_t* inode, uint64_t block, void* buf,
    size_t count)
{
    ext2_fs_t* fs = inode->fs;
    ext2_cache_block_t* b;
    size_t block_size = ext2_sb_blocksize(fs->sb);
    uint64_t offset;
    size_t i;

    for (i = 0; i < count; i++) {
        offset = get_block_offset(inode, block + i, 0);

        // Ein paar Nullen fuer Sparse Files
        if (offset == 0) {
            memset(buf + block_size * i, 0, block_size);
            continue;
        }

        b = fs->cache_block(fs->cache_handle, offset / block_size, 0);
        if (!b) {
            return i;
        }

        memcpy(buf + block_size * i, b->data, block_size);
        fs->cache_block_free(b, 0);
    }

    return count;
}

static int writeblk(ext2_inode_t* inode, uint64_t block, const void* buf)
{
    ext2_fs_t* fs = inode->fs;
    ext2_cache_block_t* b;
    uint64_t block_offset = get_block_offset(inode, block, 1);
    size_t   block_size = ext2_sb_blocksize(fs->sb);

    if (block_offset == 0) {
        return 0;
    }

    b = fs->cache_block(fs->cache_handle, block_offset / block_size, 1);
    if (!b) {
        return 0;
    }

    memcpy(b->data, buf, block_size);
    fs->cache_block_free(b, 1);

    return 1;
}

int ext2_inode_writeblk(ext2_inode_t* inode, uint64_t block, void* buf)
{
    size_t block_size = ext2_sb_blocksize(inode->fs->sb);
    int ret = writeblk(inode, block, buf);

    if ((block + 1) * block_size > inode->raw->size) {
        inode->raw->size = (block + 1) * block_size;
    }

    return ret;
}

int ext2_inode_readdata(
    ext2_inode_t* inode, uint64_t start, size_t len, void* buf)
{
    size_t block_size = ext2_sb_blocksize(inode->fs->sb);
    uint64_t start_block = start / block_size;
    uint64_t end_block = (start + len - 1) / block_size;
    size_t block_count = end_block - start_block + 1;
    char localbuf[block_size];
    uint32_t i;
    int ret;

    if (len == 0) {
        return 1;
    }

    // Wenn der erste Block nicht ganz gelesen werden soll, wird er zuerst in
    // einen Lokalen Puffer gelesen.
    if (start % block_size) {
        size_t bytes;
        size_t offset = start % block_size;

        if (!ext2_inode_readblk(inode, start_block, localbuf, 1)) {
            return 0;
        }

        bytes = block_size - offset;
        if (len < bytes) {
            bytes = len;
        }
        memcpy(buf, localbuf + offset, bytes);

        if (--block_count == 0) {
            return 1;
        }
        len -= bytes;
        buf += bytes;
        start_block++;
    }

    // Wenn der letzte Block nicht mehr ganz gelesen werden soll, muss er
    // separat eingelesen werden.
    if (len % block_size) {
        size_t bytes = len % block_size;

        if (!ext2_inode_readblk(inode, end_block, localbuf, 1)) {
            return 0;
        }
        memcpy(buf + len - bytes, localbuf, bytes);

        if (--block_count == 0) {
            return 1;
        }
        len -= bytes;
        end_block--;
    }

    for (i = 0; i < block_count; i++) {
        ret = ext2_inode_readblk(inode, start_block + i, buf + i * block_size,
            block_count - i);

        if (!ret) {
            return 0;
        }

        // Wenn mehrere Blocks aneinander gelesen wurden, muessen die jetzt
        // uebersprungen werden.
        i += ret - 1;
    }

    return 1;
}

int ext2_inode_writedata(
    ext2_inode_t* inode, uint64_t start, size_t len, const void* buf)
{
    size_t block_size = ext2_sb_blocksize(inode->fs->sb);
    uint64_t start_block = start / block_size;
    uint64_t end_block = (start + len - 1) / block_size;
    size_t block_count = end_block - start_block + 1;
    char localbuf[block_size];
    uint32_t i;
    int ret;
    size_t req_len = len;

    // Wenn der erste Block nicht ganz geschrieben werden soll, wird er zuerst
    // in einen Lokalen Puffer gelesen werden, damit nichts ueberschrieben wird.
    if (start % block_size) {
        size_t bytes;
        size_t offset = start % block_size;

        if (!ext2_inode_readblk(inode, start_block, localbuf, 1)) {
            return 0;
        }

        bytes = block_size - offset;
        if (len < bytes) {
            bytes = len;
        }
        memcpy(localbuf + offset, buf, bytes);

        if (!writeblk(inode, start_block, localbuf)) {
            return 0;
        }

        if (--block_count == 0) {
            goto out;
        }
        len -= bytes;
        buf += bytes;
        start_block++;
    }

    // Wenn der letzte Block nicht mehr ganz geschrieben werden soll, muss er
    // zuerst eingelesen werden, damit nichts ueberschrieben wird.
    if (len % block_size) {
        size_t bytes = len % block_size;

        if (!ext2_inode_readblk(inode, end_block, localbuf, 1)) {
            return 0;
        }
        memcpy(localbuf, buf + len - bytes, bytes);

        if (!writeblk(inode, end_block, localbuf)) {
            return 0;
        }

        if (--block_count == 0) {
            goto out;
        }
        len -= bytes;
        end_block--;
    }

    for (i = 0; i < block_count; i++) {
        ret = writeblk(inode, start_block + i, buf + i * block_size);

        if (!ret) {
            return 0;
        }
    }

out:
    if (start + req_len > inode->raw->size) {
        inode->raw->size = start + req_len;
    }

    return 1;
}

int ext2_inode_truncate(ext2_inode_t* inode, uint64_t size)
{
    size_t block_size = ext2_sb_blocksize(inode->fs->sb);
    uint64_t first_to_free = (size / block_size) + 1;
    uint64_t last_to_free =
        ((inode->raw->size + block_size - 1) / block_size);
    uint64_t i;

    for (i = first_to_free; i <= last_to_free; i++) {
        get_block_offset(inode, i, 2);
    }

    inode->raw->size = size;

    if (!ext2_inode_update(inode)) {
        return 0;
    }

    return 1;
}

#define DL(n,v) printf("   %s: %d %u 0x%x\n", n, (int) (inode->raw->v), \
    (unsigned int) (inode->raw->v), (unsigned int) (inode->raw->v));
void ext2_inode_dump(ext2_inode_t* inode)
{
    printf("Inode %d:\n", inode->number);
    DL("size", size);
    DL("block_count", block_count);
    DL("block0", blocks[0]);
    DL("block1", blocks[1]);
    DL("block2", blocks[2]);
    DL("block3", blocks[3]);
    DL("block4", blocks[4]);
    DL("block5", blocks[5]);
    DL("block6", blocks[6]);
    DL("block7", blocks[7]);
    DL("block8", blocks[8]);
    DL("block9", blocks[9]);
    DL("block10", blocks[10]);
    DL("block11", blocks[11]);
    DL("block12", blocks[12]);
    DL("block13", blocks[13]);

}
