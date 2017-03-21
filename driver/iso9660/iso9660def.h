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

#ifndef _ISO9660DEF_H_
#define _ISO9660DEF_H_

// Default sector size. Used to load volume descriptors
#define ISO9660_DEFAULT_SECTOR_SIZE 2048

// Number of first used sector (first volume descriptor)
#define ISO9660_FIRST_SECTOR        16

// Define if you want to use lower filenames
// (if you don't use an extension that supports lower filenames)
#define ISO9660_LOWER_FILENAMES

#ifndef ISO9660_USE_ROCKRIDGE
  // Define if you want to use Rockridge extension
  #define ISO9660_USE_ROCKRIDGE
#endif

#ifndef DEBUG
  //#define DEBUG stderr
#endif

#endif
