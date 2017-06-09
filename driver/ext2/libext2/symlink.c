/*
 * Copyright (c) 2007 The tyndur Project. All rights reserved.
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
#include <stdlib.h>
#include <string.h>

#include "ext2.h"

/**
 * Neuen Symlink erstellen
 *
 * @param parent Inode des Elternverzeichnisses
 * @param name Name des neuen Symlinks
 * @param target Ziel des neuen Symlinks
 * @param newi Neu anzulegender Inode
 *
 * @return 1 bei Erfolg, 0 im Fehlerfall
 */
int ext2_symlink_create(ext2_inode_t* parent,
    const char* name, const char* target, ext2_inode_t* newi)
{
    // Zuerst wird mal ein neuer Inode alloziert
    if (!ext2_inode_alloc(parent->fs, newi)) {
        return 0;
    }

    // Verzeichniseintrag anlegen
    if (!ext2_dir_link(parent, newi, name)) {
        goto fail;
    }

    // Initialisierung des Inode
    newi->raw->deletion_time = 0;
    newi->raw->mode = EXT2_INODE_MODE_SYMLINK | 0777;
    if (!ext2_inode_update(newi) ||
        !ext2_inode_update(parent))
    {
        return 0;
    }

    // Ziel in den Symlink schreiben
    if (!ext2_inode_writedata(newi, 0, strlen(target), target)) {
        return 0;
    }

    if (!ext2_inode_update(newi)) {
        return 0;
    }

    return 1;

fail:
    ext2_inode_free(newi);
    return 0;
}

/**
 * Pfad aus einem Symlink in einen frisch allozierten Buffer auslesen.
 * Der Aufrufer ist dafuer zustaendig, den Buffer wieder freizugeben.
 *
 * @param inode Inode des auszulesenden Symlinks
 */
char* ext2_symlink_read(ext2_inode_t* inode)
{
    char* result = malloc(inode->raw->size + 1);
    result[inode->raw->size] = '\0';

    // Bei Fast Symlinks wird das block-Array zur Speicherung des Pfades
    // benutzt, die Block-Anzahl ist dann 0
    if (inode->raw->block_count == 0) {
        // Wir haben es mit einem Fast Symlink zu tun, da ist das ganze sehr
        // einfach: Nur den String aus dem Blockarray in den Puffer kopieren
        // und gut ist.
        memcpy(result, inode->raw->blocks, inode->raw->size);
    } else {
        // Normale symlinks werden gleich wie regulaere Dateien behandelt
        size_t res = ext2_inode_readdata(inode, 0, inode->raw->size, result);

        if (!res) {
            free(result);
            return NULL;
        }
    }

    return result;
}

/**
 * Pfad in einem Symlink aendern
 *
 * @param inode Inode des zu schreibenden Symlinks
 * @param path  Neuer Pfad
 *
 * @return 1 bei Erfolg 0 sonst
 */
int ext2_symlink_write(ext2_inode_t* inode, const char* path)
{
    inode->raw->size = strlen(path);

    if (inode->raw->size < 15 * 4 - 1) {
        strcpy((char*) inode->raw->blocks, path);
    } else {
        // Normale symlinks werden gleich wie regulaere Dateien behandelt
        size_t res = ext2_inode_writedata(inode, 0, inode->raw->size, path);

        if (!res) {
            return 0;
        }
    }

    return 1;
}

