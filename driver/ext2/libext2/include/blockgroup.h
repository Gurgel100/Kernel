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

#ifndef _BLOCKGROUP_H_
#define _BLOCKGROUP_H_

#include <stdint.h>

/// Gruppen deskriptor
typedef struct ext2_blockgroup {
    /// Nummer des Blocks mit der Block-Allokationsbitmap
    uint32_t block_bitmap;

    /// Nummer des Blocks mit der Inode-Allokationsbitmap
    uint32_t inode_bitmap;

    /// Nummer des Blocks mit der Inode-Tabelle
    uint32_t inode_table;

    /// Anzahl der freien Blöcke
    uint16_t free_blocks;

    /// Anzahl der freien Inodes
    uint16_t free_inodes;

    /// Anzahl der Verzeichnisse
    uint16_t used_directories;
    uint16_t padding;

    /// Reserviert
    uint32_t reserved[3];
} __attribute__((packed)) ext2_blockgroup_t;

/**
 * Blockgruppendeskriptor einlesen
 *
 * @param fs        Dateisystem
 * @param group_nr  Nummer der Blockgruppe
 * @param bg        Pointer auf Speicherbereich in dem die geladene
 *                  Blockgruppe abgespeichert werden soll.
 *
 * @return 1 wenn die Blockgruppe erfolgreich eingelesen wurde, 0 sonst
 */
int ext2_bg_read(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg);

/**
 * Blockgruppendeskriptor sichern
 *
 * @param fs        Dateisystem
 * @param group_nr  Nummer der Blockgruppe
 * @param bg        Pointer auf Speicherbereich aus dem der Deskriptor gelesen
 *                  werden soll
 *
 * @return 1 wenn die Blockgruppe geschrieben wurde, 0 sonst.
 */
int ext2_bg_update(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg);

#endif
