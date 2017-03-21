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

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/**
 * Pfad aus einem Symlink in einen frisch allozierten Buffer auslesen.
 * Der Aufrufer ist dafuer zustaendig, den Buffer wieder freizugeben.
 *
 * @param inode Inode des auszulesenden Symlinks
 */
char* ext2_symlink_read(ext2_inode_t* inode);

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
    const char* name, const char* target, ext2_inode_t* newi);

/**
 * Pfad in einem Symlink aendern
 *
 * @param inode Inode des zu schreibenden Symlinks
 * @param path  Neuer Pfad
 *
 * @return 1 bei Erfolg 0 sonst
 */
int ext2_symlink_write(ext2_inode_t* inode, const char* path);

#endif
