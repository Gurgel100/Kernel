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
#include <stdio.h>

#include <string.h>
#include "ext2.h"

/**
 * Prueft ob eine Zahl ein vielfaches von einer anderen ist
 *
 * @param x Zahl von der in Erfahrung gebracht werden soll ob sie ein
 *          viellfaches von y ist oder nicht.
 * @param y
 *
 * @return 1 wenn die Zahl ein Vielfaches ist, 0 sonst
 */
static int test_root(uint32_t x, uint32_t y)
{
    uint32_t current = y;

    while (current < x) {
        current *= y;
    }

    return (current == x);
}


/**
 * Prueft ob eine Blockgruppe eine Superblockkopie enthaelt
 *
 * @param sb Superblock des Dateisystems
 * @param bg Nummer der Blockgruppe
 *
 * @return 1 wenn sie eine Kopie hat, 0 sonst
 */
static int bg_has_sb(ext2_superblock_t* sb, int bg)
{
    // Wenn das feature Sparse Super nicht gesetzt ist, enthalten alle
    // Blockgruppen eine Kopie
    if ((sb->features_ro & EXT2_FEATURE_ROCOMP_SPARSE_SUPER) == 0) {
        return 1;
    }

    // Mit SPARSE_SUPER ist die Entscheidung etwas komplizierter. Da werden
    // naemlich Kopien in allen Gruppen abgespeichert, deren Nummer ein
    // Vielfaches von 3, 5 oder 7 sind. Auch in der Ersten Gruppe ist dann eine
    // Kopie
    return ((bg <= 1) || test_root(bg, 3) || test_root(bg, 5) ||
        test_root(bg, 7));
}


/**
 * Liest den Superblock vom angegebenen Offset auf der Platte und prueft ob die
 * Magic stimmt.
 *
 * @param fs    Dateisystem
 * @param sb    Pointer auf Speicherbereich in dem der Superblock abgelegt
 *              werden soll
 * @param start Offset auf der Platte
 *
 * @return 1 bei Erfolg, 0 sonst
 */
static int sb_read_from(ext2_fs_t* fs, ext2_superblock_t* sb, uint64_t start)
{
    if (!fs->dev_read(start, sizeof(ext2_superblock_t), sb, fs->dev_private)) {
        return 0;
    }

    if (sb->magic != EXT2_SB_MAGIC) {
        return 0;
    }

    return 1;
}

int ext2_sb_read(ext2_fs_t* fs, ext2_superblock_t* sb)
{
    return sb_read_from(fs, sb, 1024);
}

int ext2_sb_update(ext2_fs_t* fs, ext2_superblock_t* sb)
{
    uint64_t start;
    int i;
    int group_count = (sb->block_count + sb->blocks_per_group - 1) / sb->
        blocks_per_group;
    size_t block_size = ext2_sb_blocksize(fs->sb);
    ext2_cache_block_t* block;

    for (i = 0; i < group_count; i++) {
        if (!bg_has_sb(fs->sb, i)) {
            continue;
        }

        start = i == 0 ? 1024 : (fs->sb->first_data_block + i * fs->sb->
            blocks_per_group) * block_size;

        block = fs->cache_block(fs->cache_handle, start / block_size, 1);
        if (i == 0 && block_size > 1024) {
            memcpy(block->data, fs->boot_sectors, 1024);
            memcpy(block->data + 1024, sb, sizeof(ext2_superblock_t));
            memset(block->data + 1024 + sizeof(ext2_superblock_t), 0,
                   block_size - 1024 - sizeof(ext2_superblock_t));
        } else if (i == 0) {
            memcpy(block->data, sb, sizeof(ext2_superblock_t));
        } else {
            memcpy(block->data, sb, sizeof(ext2_superblock_t));
            memset(block->data + sizeof(ext2_superblock_t), 0,
                   block_size - sizeof(ext2_superblock_t));
        }
        fs->cache_block_free(block, 1);
    }

    return 1;
}

int ext2_sb_copy_count(ext2_superblock_t* sb)
{
    int count = 0;
    int i;
    int group_count = (sb->block_count + sb->blocks_per_group - 1) / sb->
        blocks_per_group;

    for (i = 1; i < group_count; i++) {
        count += bg_has_sb(sb, i);
    }
    return count;
}

int ext2_sb_read_copy(ext2_fs_t* fs, ext2_superblock_t* dest, int copy)
{
    int i;
    int group_count = (fs->sb->block_count + fs->sb->blocks_per_group - 1) /
        fs->sb->blocks_per_group;
    int count = 0;
    uint64_t start;

    for (i = 1; (i <= group_count) && (count <= copy); i++) {
        if (bg_has_sb(fs->sb, i)) {
            if (count == copy) {
                start = (fs->sb->first_data_block + i * fs->sb->
                    blocks_per_group) * ext2_sb_blocksize(fs->sb);
                return sb_read_from(fs, dest, start);
            }
            count++;
        }
    }
    return 0;
}

#define abcmp(x) (a->x == b->x)
int ext2_sb_compare(ext2_superblock_t* a, ext2_superblock_t* b)
{
    return (abcmp(inode_count) && abcmp(block_count) && abcmp(free_blocks) &&
        abcmp(free_inodes) && abcmp(first_data_block) && abcmp(block_size) &&
        abcmp(blocks_per_group) && abcmp(inodes_per_group) && abcmp(magic) &&
        abcmp(revision) && abcmp(inode_size) && abcmp(features_compat) &&
        abcmp(features_incompat) && abcmp(features_ro));
}

void ext2_sb_dump(ext2_superblock_t* sb)
{
    printf("  sb->inode_count = %d\n", sb->inode_count);
    printf("  sb->block_count = %d\n", sb->block_count);
    printf("  sb->free_blocks = %d\n", sb->free_blocks);
    printf("  sb->free_inodes = %d\n", sb->free_inodes);
    printf("  sb->first_data_block = %d\n", sb->first_data_block);
    printf("  sb->block_size = %d\n", sb->block_size);
    printf("  sb->blocks_per_group = %d\n", sb->blocks_per_group);
    printf("  sb->inodes_per_group = %d\n", sb->inodes_per_group);
    printf("  sb->magic = %d\n", sb->magic);
    printf("  sb->revision = %d\n", sb->revision);
    printf("  sb->inode_size = %d\n", sb->inode_size);
    printf("  sb->features_compat = %d\n", sb->features_compat);
    printf("  sb->features_incompat = %d\n", sb->features_incompat);
    printf("  sb->features_ro = %d\n", sb->features_ro);
}


