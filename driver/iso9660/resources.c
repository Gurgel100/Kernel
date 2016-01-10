/*
    iso9660 - An iso9660 CDI driver with Rockridge support
    Copyright (C) 2008  Janosch Gräf <janosch.graef@gmx.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "iso9660_cdi.h"

struct cdi_fs_res_res iso9660_fs_res_res = {
    .load = iso9660_fs_res_load,
    .unload = iso9660_fs_res_unload,
    .meta_read = iso9660_fs_res_meta_read,
};

struct cdi_fs_res_file iso9660_fs_res_file = {
    // Prinzipiell haben wir nur ausfuehrbare Dateien, der Rest wird mit den
    // Berechtigungen geregelt
    .executable = 1,

    .read = iso9660_fs_file_read,
};

struct cdi_fs_res_dir iso9660_fs_res_dir = {
    .list = iso9660_fs_dir_list,
};
