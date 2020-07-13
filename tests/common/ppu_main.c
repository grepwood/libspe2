/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2008 Sony Computer Entertainment Inc.
 *  Copyright 2007,2008 Sony Corp.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ppu_libspe2_test.h"

#define PROGRESS_STEP 1000

int g_failed;

int ppu_main(int argc, char **argv, int (*test_proc)(int targc, char **targv))
{
  int i;

  /* run main test routine in 'iteration' times */
  for (i = 0; NUM_ITERATIONS < 0 || i != NUM_ITERATIONS; i++) {
    if (i % PROGRESS_STEP == 0) {
      tprintf("[%d/%d]\n", i, NUM_ITERATIONS);
    }
    if ((*test_proc)(argc, argv)) {
      return 1;
    }
    check_failed();
  }

  tprintf("done\n");

  return 0;
}
