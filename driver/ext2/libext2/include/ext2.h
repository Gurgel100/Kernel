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

#ifndef _EXT2_H_
#define _EXT2_H_

#include <stdint.h>
#include <stdlib.h>

struct ext2_superblock_t;

/**
 * Zentrale Struktur zur Verwaltung eines eingebundenen Dateisystemes
 */
typedef struct ext2_fs {
    /**
     * Funktionspointer der von Aufrufer gesetzt werden muss. Diese Funktion
     * liest Daten vom Datentraeger ein, auf dem sich das Dateisystem befindet.
     *
     * @param start Position von der an gelesen werden soll
     * @param size  Anzahl der zu lesenden Bytes
     * @param dest  Pointer auf den Puffer in dem die Daten abgelegt werden
     *              sollen
     * @param prv   Private Daten zum Zugriff auf den Datentraeger (z.B.
     *              Dateideskriptor), aus dev_private zu entnehmen
     *
     * @return 1 wenn das Lesen geklappt hat, 0 sonst
     */
    int (*dev_read)(uint64_t start, size_t size, void* dest, void* prv);

    /**
     * Funktionspointer der von Aufrufer gesetzt werden muss. Diese Funktion
     * schreibt Daten auf den Datentraeger, auf dem sich das Dateisystem
     * befindet.
     *
     * @param start Position von der an gelesen werden soll
     * @param size  Anzahl der zu lesenden Bytes
     * @param dest  Pointer auf den Puffer, aus dem die Daten gelesen werden
     *              sollen.
     * @param prv   Private Daten zum Zugriff auf den Datentraeger (z.B.
     *              Dateideskriptor), aus dev_private zu entnehmen
     *
     * @return 1 wenn das Schreiben geklappt hat, 0 sonst
     */
    int (*dev_write)(uint64_t start, size_t size, const void* source,
        void* prv);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * erstellt einen neuen Cache und gibt ein Handle darauf zurueck.
     *
     * @param fs            Pointer auf das Dateisystem fuer das der Cache
     *                      erstellt werden soll.
     * @param block_size    Blockgroesse im Cache
     *
     * @return Handle oder NULL im Fehlerfall
     */
    void* (*cache_create)(struct ext2_fs* fs, size_t block_size);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * zerstoert einen mit cache_create erstellten Cache
     *
     * @param cache Handle
     */
    void (*cache_destroy)(void* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * schreibt alle veraenderten Cache-Blocks auf die Platte
     *
     * @param cache Handle
     */
    void (*cache_sync)(void* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * holt einen Block aus dem Cache und gibt einen Pointer auf ein
     * Block-Handle zrueck. Dieser Pointer ist mindestens solange gueltig, wie
     * das Handle nicht freigegeben wird.
     *
     * @param cache     Cache-Handle
     * @param block     Blocknummer
     * @param noread    Wenn dieser Parameter != 0 ist, wird der Block nicht
     *                  eingelesen. Das kann benutzt werden, wenn er eh
     *                  vollstaendig ueberschrieben wird.
     *
     * @return Block-Handle
     */
    struct ext2_cache_block* (*cache_block)(void* cache, uint64_t block,
        int noread);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * markiert den angegebenen Block als veraendert, damit er geschrieben wird,
     * bevor er aus dem Cache entfernt wird.
     *
     * @param handle Block-Handle
     */
    void (*cache_block_dirty)(struct ext2_cache_block* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * gibt einen mit cache_block alloziierten Block wieder frei.
     *
     * @param handle    Block-Handle
     * @param dirty     Wenn != 0 wird der Block auch als Dirty markiert
     */
    void (*cache_block_free)(struct ext2_cache_block* handle, int dirty);


    /// Private Daten zum Zugriff auf den Datentraeger
    void* dev_private;

    /// Handle fuer Blockcache
    void* cache_handle;

    /// Superblock des Dateisystems
    struct ext2_superblock_t* sb;

    /// Blocknummer in der der erste Superblock liegt
    uint64_t sb_block;

    /// Darf vom Aufrufer benutzt werden
    void* opaque;

    /// Letzte allozierte Blocknummer
    uint64_t block_prev_alloc;

    /// Der Inhalt der ersten 1024 Bytes auf dem Gerät
    void* boot_sectors;
} ext2_fs_t;

/**
 * Cache-Block-Handle
 */
typedef struct ext2_cache_block {
    /// Blocknummer
    uint64_t number;

    /// Daten
    void* data;

    /// Cache-Handle
    void* cache;

    /// Daten fuer die Implementierung
    void* opaque;
} ext2_cache_block_t;

/**
 * ext2-Dateisystem einbinden. Dafuer muessen dev_read, dev_write und
 * dev_private (falls notwendig) initialisiert sein.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int ext2_fs_mount(ext2_fs_t* fs);

/**
 * ext2-Dateisystem aushaengen. Dabei werden saemtliche gecachten Daten auf die
 * Platte geschrieben. Die Member sb und cache_handle in der fs-Struktur sind
 * danach NULL.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int ext2_fs_unmount(ext2_fs_t* fs);

/**
 * Saetmliche gecachten Daten auf die Platte schreiben
 */
void ext2_fs_sync(ext2_fs_t* fs);


#include "superblock.h"
#include "blockgroup.h"
#include "inode.h"
#include "directory.h"
#include "file.h"
#include "symlink.h"

#endif
