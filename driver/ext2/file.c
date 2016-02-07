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

size_t ext2_fs_file_read(struct cdi_fs_stream* stream, uint64_t start,
    size_t size, void* data)
{
    struct ext2_fs_res* res = (struct ext2_fs_res*) stream->res;

    if (ext2_inode_readdata(res->inode, start, size, data)) {
        return size;
    }

    //TODO: error in stream setzen
    return 0;
}

size_t ext2_fs_file_write(struct cdi_fs_stream* stream, uint64_t start,
    size_t size, const void* data)
{

    struct ext2_fs_res* res = (struct ext2_fs_res*) stream->res;

    //TODO: data muesste in writedata const werden
    if (ext2_inode_writedata(res->inode, start, size, (void*) data)) {
        ext2_inode_update(res->inode);
        return size;
    }

    //TODO: error in stream setzen
    return 0;
}

int ext2_fs_file_truncate(struct cdi_fs_stream* stream, uint64_t size)
{
    struct ext2_fs_res* res = (struct ext2_fs_res*) stream->res;

    if (res->inode->raw->size == 0) {
        return 1;
    }

    // TODO: Vergroessern tut damit nicht
    return ext2_inode_truncate(res->inode, size);
}

