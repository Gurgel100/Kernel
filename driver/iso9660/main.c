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

#include <stdio.h>
#include <stdarg.h>

#include "cdi/fs.h"

#include "iso9660_cdi.h"
#include "iso9660def.h"

#define DRIVER_NAME "iso9660"

static struct cdi_fs_driver iso9660_driver;

/**
 * Initializes the data structures for the iso9660 driver
 */
static int iso9660_driver_init(void) {
    // Konstruktor der Vaterklasse
    cdi_fs_driver_init(&iso9660_driver);

    return 0;
}

/**
 * Deinitialize the data structures for the iso9660 driver
 */
static int iso9660_driver_destroy(void)
{
    cdi_fs_driver_destroy(&iso9660_driver);
    return 0;
}

/**
 * If DEBUG is definded, it outputs the debug message onto the stream
 * defined with DEBUG
 *  @param fmt Format (see printf)
 *  @param ... Parameters
 *  @return Amount of output characters
 */
int debug(const char *fmt,...) {
#ifdef DEBUG
  va_list args;
  va_start(args,fmt);
  int ret = vfprintf(DEBUG,fmt,args);
  va_end(args);
  return ret;
#else
  return 0;
#endif
}


static struct cdi_fs_driver iso9660_driver = {
    .drv = {
        .type           = CDI_FILESYSTEM,
        .name           = DRIVER_NAME,
        .init           = iso9660_driver_init,
        .destroy        = iso9660_driver_destroy,
    },
    .fs_init            = iso9660_fs_init,
    .fs_destroy         = iso9660_fs_destroy,
    .fs_probe           = iso9660_fs_probe,
};

CDI_DRIVER(DRIVER_NAME, iso9660_driver)
