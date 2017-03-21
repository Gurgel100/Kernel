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

#ifndef _INODE_H_
#define _INODE_H_

#include <stdint.h>
#include "ext2.h"

/// Inode (On-Disk-Format)
typedef struct ext2_raw_inode_t {
    /// Dateimodus
    uint16_t mode;

    /// UID des Besitzers
    uint16_t uid;

    /// Groesse
    uint32_t size;

    /// Zeit des letzten Zugriffs
    uint32_t access_time;

    /// Erstellungszeit;
    uint32_t creation_time;

    /// Zeit der letzten Aenderungn
    uint32_t modification_time;

    /**
     * Zeit der Loeschung. 0 fuer existierende Dateien. Bis zu einem Wert von
     * superblock.inode_count - 1 als Next-Pointer der Orphaned-Liste genutzt.
     */
    uint32_t deletion_time;

    /// GID
    uint16_t gid;

    /// Anzahl der Links
    uint16_t link_count;

    /// Anzahl der Blocks.
    /// ACHTUNG: HIER HANDELT ES SICH UM 512-BYTE GROSSE BLOECKE!!!
    uint32_t block_count;

    /// Flags
    uint32_t flags;

    /// Reserviert
    uint32_t reserved1;

    /**
     *  Blocknummern. Fuer Blocks 0 bis 11 zeigt die Nummer direkt auf einen
     *  Datenblock der Datei, Block 12 ist indirekt, 13 doppelt indirekt und 14
     *  dreifach indirekt. Bei indirekter Adressierung zeigt die Blocknummer
     *  auf einen Block, der wiederum Blocknummern enthaelt.
     */
    uint32_t blocks[15];

    /// Version
    uint32_t version;

    /// TODO
    uint32_t file_acl;

    /// TODO
    uint32_t dir_acl;


    /// Fragment Adresse...
    uint32_t fragment_address;

    /// Fragment Nummer
    uint8_t fragment_number;

    /// Fragmentgroesse
    uint8_t fragment_size;

    uint16_t reserved2[5];
} __attribute__((packed)) ext2_raw_inode_t;

typedef struct ext2_inode_t {
    /// Dateisystem, zu dem der Inode gehoert
    ext2_fs_t* fs;

    /// Nummer des Inode
    uint32_t number;

    /// Datenstruktur auf der Platte
    struct ext2_raw_inode_t* raw;

    /// Cache-Block
    ext2_cache_block_t* block;
} ext2_inode_t;



// Makros fuer das Mode-Feld in Inodes
#define EXT2_INODE_MODE_FORMAT 0xF000
#define EXT2_INODE_MODE_SYMLINK 0xA000
#define EXT2_INODE_MODE_FILE 0x8000
#define EXT2_INODE_MODE_DIR 0x4000

#define EXT2_INODE_IS_FORMAT(i, f) \
    (((i)->raw->mode & EXT2_INODE_MODE_FORMAT) == f)
#define EXT2_INODE_IS_SYMLINK(i) EXT2_INODE_IS_FORMAT(i, \
    EXT2_INODE_MODE_SYMLINK)
#define EXT2_INODE_IS_FILE(i) EXT2_INODE_IS_FORMAT(i, EXT2_INODE_MODE_FILE)
#define EXT2_INODE_IS_DIR(i) EXT2_INODE_IS_FORMAT(i, EXT2_INODE_MODE_DIR)

typedef enum {
    EXT2_IT_UNKOWN = 0,
    EXT2_IT_FILE = 1,
    EXT2_IT_DIR = 2,
    EXT2_IT_SYMLINK = 7
} ext2_inode_type_t;


/**
 * Inode einlesen
 *
 * @param fs        Dateisystem
 * @param inode_nr  Nummer des zu ladenden Inodes
 * @param inode     Pointer auf Speicherbereich in den der Inode geladen
 *                  werden soll
 *
 * @return 1 wenn der Inode eingelesen wurde, 0 sonst.
 */
int ext2_inode_read(ext2_fs_t* fs, uint64_t inode_nr, ext2_inode_t* inode);

/**
 * Eingelesenen oder neu erstellten Inode im Speicher freigeben
 *
 * @param inode Freizugebender Inode
 */
void ext2_inode_release(ext2_inode_t* inode);

/**
 * Inode speichern
 *
 * @param inode Mit inode_read gelesener/mit inode_alloc allozierter Inode
 *
 * @return 1 wenn der Inode eingelesen wurde, 0 sonst.
 */
int ext2_inode_update(ext2_inode_t* inode);

/**
 * Neuen Inode allozieren
 *
 * @param fs    Dateisystem
 * @param inode Speicherbreich in dem der allozierte Inode abgelegt werden
 *              soll.
 *
 * @return 1 wenn der Inode erfolgreich alloziert wurde, 0 sonst
 */
int ext2_inode_alloc(ext2_fs_t* fs, ext2_inode_t* inode);

/**
 * Inode freigeben
 *
 * @param inode Freizugebender Inode
 *
 * @return 1 wenn der Inode erfolgreich freigegeben wurde, 0 sonst
 */
int ext2_inode_free(ext2_inode_t* inode);

/**
 * Aneinanderhaengende Datenblocks aus einem Inode einlesen
 *
 * @param inode Inode aus dem die Blocks geladen werden sollen
 * @param block Blocknummer des Ersten
 * @param buf   Puffer in den die Blocks gelesen werden sollen
 * @þaram count Anzahl der Blocks die maximal gelesen werden sollen
 *
 * @return Anzahl der gelesenen Blocks
 */
int ext2_inode_readblk(ext2_inode_t* inode, uint64_t block, void* buf,
    size_t count);

/**
 * Datenblock in einen Inode schreiben
 *
 * @param inode Inode in den der Block geschrieben werden soll
 * @param block Blocknummer
 * @param buf   Puffer aus dem die Daten geschrieben werden sollen
 *
 * @return 1 wenn der Block erfolgreich geschrieben wurde, 0 sonst
 */
int ext2_inode_writeblk(ext2_inode_t* inode, uint64_t block, void* buf);

/**
 * Datenbereich aus einem Inode lesen
 *
 * @param inode Inode
 * @param start Offset vom Anfang der Daten
 * @param len   Anzahl der zu lesenden Bytes
 * @param buf   Pufer in dem die Daten abgelegt wurden
 *
 * @return 1 wenn die Daten erfolgreich gelesen wurden, 0 sonst
 */
int ext2_inode_readdata(
    ext2_inode_t* inode, uint64_t start, size_t len, void* buf);

/**
 * Datenbereich in einen Inode schreiben
 *
 * @param inode Inode
 * @param start Offset an den die Daten geschrieben werden
 * @param len   Anzahl der zu schreibenden Bytes
 * @param buf   Puffer aus dem die Daten geschreiben werden sollen
 *
 * @return 1 wenn die Daten erfolgreich geschrieben wurden, 0 sonst
 */
int ext2_inode_writedata(
    ext2_inode_t* inode, uint64_t start, size_t len, const void* buf);

/**
 * Laenge der Daten eines Inode aendern
 */
int ext2_inode_truncate(ext2_inode_t* inode, uint64_t size);

/**
 * Typ eines Inodes bestimmen
 *
 * @param inode Pointer auf den Inode
 *
 * @return Typ
 */
static inline ext2_inode_type_t ext2_inode_type(ext2_inode_t* inode) {
    switch (inode->raw->mode & EXT2_INODE_MODE_FORMAT) {
        case EXT2_INODE_MODE_DIR:
            return EXT2_IT_DIR;

        case EXT2_INODE_MODE_FILE:
            return EXT2_IT_FILE;

        case EXT2_INODE_MODE_SYMLINK:
            return EXT2_IT_SYMLINK;

        default:
            return EXT2_IT_UNKOWN;
    }
}

/**
 * Interne Nummer errechnen, mit der gerechnet werden muss
 *
 * @param fs        Dateisystem
 * @param nr        Inodenummer
 *
 * @return Nummer
 */
static inline uint64_t ext2_inode_to_internal(ext2_fs_t* fs, uint64_t nr) {
    return nr - 1;
}

/**
 * Aus interner Inode nummer die richtige Inodenummer errechnen, die zum
 * Beispiel in Verzeichniseintraegen eingetragen wird.
 *
 * @param fs        Dateisystem
 * @param nr        Inodenummer
 *
 * @return Nummer
 */
static inline uint64_t ext2_inode_from_internal(ext2_fs_t* fs, uint64_t nr) {
    return nr + 1;
}

/**
 * Inhalt der wichtigsten Felder eines Inodes auf der Standardausgabe anzeigen
 *
 * @param inode Pointer auf den Inode
 */
void ext2_inode_dump(ext2_inode_t* inode);

#endif
