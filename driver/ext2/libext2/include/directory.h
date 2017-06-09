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

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

#include <stdint.h>

/// Inodenummer des Rootverzeichnisses
#define EXT2_ROOTDIR_INODE 2

/// Verzeichniseintrag wie er intern benutzt wird
typedef struct ext2_dirent {
    /// Inode-Nummer
    uint32_t inode;

    /// Laenge des aktuellen Eintrags
    uint16_t record_len;

    /// Laenge des Namens
    uint8_t name_len;

    /// Dateityp
    uint8_t type;

    /// Name (_NICHT_ nullterminiert)
    char name[];
} __attribute__((packed)) ext2_dirent_t;


/**
 * Verzeichniseintraege verabeiten
 *
 * @param inode     Pointer auf den Verzeichnis-Inode
 * @param handler   Handler, der fuer jeden einzelnen Verzeichniseintrag
 *                  aufgerufen wird. Der erste Parameter der Funktion ist ein
 *                  Zeiger auf den Verzeichniseintrag, der zweite erhaelt den
 *                  Wert private.
 * @param private   Wert der direkt an den Handler uebergeben wird.
 */
void ext2_dir_foreach(ext2_inode_t* inode,
    int (*handler) (ext2_dirent_t*, void*), void* private);

/**
 * Sucht einen Verzeichniseintrag nach dem Namen
 *
 * @param inode Pointer auf den Inode des Elternverzeichnisses
 * @param name  Namen des zu suchenden Eintrages
 *
 * @return      Pointer auf den gefundenen Verzeichniseintrag. Muss mit free
 *              freigegeben.
 */
ext2_dirent_t* ext2_dir_get(ext2_inode_t* inode, const char* name);

/**
 * Sucht einen Inode nach dem Pfad
 *
 * @param fs    Dateisystem auf dem gesucht werden soll
 * @param path  Gesuchter Pfad (mit / getrennt, fuehrender / optional)
 * @param inode Pointer auf Speicherstelle in der der gefundene Inode abgelegt
 *              werden soll.
 *
 * @return      1 wenn die Datei/das Verzeichnis gefunden wurde
 */
int ext2_dir_find(ext2_fs_t* fs, const char* path, ext2_inode_t* inode);

/**
 * Legt einen neuen Verzeichniseintrag an
 *
 * @param dir_inode Inode des Verzeichnisses, in dem der Eintrag erstellt
 *                  werden soll
 * @param inode     Inode des neuen Eintrags
 * @param name      Name des neuen Eintrags
 *
 * @return          1 bei Erfolg, sonst 0
 */
int ext2_dir_link(ext2_inode_t* dir_inode, ext2_inode_t* inode,
    const char* name);

/**
 * Loescht einen Verzeichniseintrag. Falls der Eintrag die letzte Referenz
 * war, wird der zugehoerige Inode geloescht.
 *
 * @param dir       Inode des Verzeichnisses, in dem der Eintrag geloescht
 *                  werden soll
 * @param inode     Inode des neuen Eintrags
 * @param name      Name des neuen Eintrags
 *
 * @return          1 bei Erfolg, sonst 0
 */
int ext2_dir_unlink(ext2_inode_t* dir, const char* name);

/**
 * Nullterminierten String allozieren und mit dem Namen eines
 * Verzeichniseintrags fuellen
 *
 * @param dirent    Verzeichnieintrag
 *
 * @return          Pointer auf allozierte Zeichenkette, muss mit free()
 *                  freigegeben werden.
 */
char* ext2_dir_alloc_name(ext2_dirent_t* dirent);


/**
 * Neues Verzeichnis erstellen
 *
 * @param parent    Inode des Elternverzeichnisses
 * @param name      Name des neuen Verzeichnisses
 * @param newi      Speicher fuer Inode des neuen Verzeichnisses
 *
 * @return          1 bei Erfolg, 0 sonst
 */
int ext2_dir_create(ext2_inode_t* parent, const char* name, ext2_inode_t* newi);

#endif
