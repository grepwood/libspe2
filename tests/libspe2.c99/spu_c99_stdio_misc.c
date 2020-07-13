/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2007 Sony Computer Entertainment Inc.
 *  Copyright 2007 Sony Corp.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* NOTE:
 *   Don't call 'stdio' functions to display error messages!!!
 *   This is a test of stdio!
 */

#include <stdio.h>

#include "spu_c99_test.h"

DEFINE_SPE_ERROR_INFO;

#define FILENAME_RENAME_FROM "__c99_stdio_misc_rename_from.tmp"
#define FILENAME_RENAME_TO   "__c99_stdio_misc_rename_to.tmp"
#define FILENAME_REMOVE      "__c99_stdio_misc_remove.tmp"

int main()
{
  /* remove, rename */
  if (rename(FILENAME_RENAME_FROM, FILENAME_RENAME_TO)) {
    error_info_exit(1);
  }

  if (remove(FILENAME_REMOVE)) {
    error_info_exit(1);
  }

  /* system ... not implemented yet. */

  /* tmpnam ... not implemented (obsolete). */

  return 0;
}
