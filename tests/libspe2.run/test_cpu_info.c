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

/* This test checks if the spe_cpu_info_get function works
 * correctly. */

#include <string.h>

#include "ppu_libspe2_test.h"


static int test(int argc, char **argv)
{
  int ret;
  int i;
  int bes;

  bes = spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -1);
  if (bes < 0) {
    eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -1): %s\n", strerror(errno));
    failed();
  }
  else if (bes == 0) {
    eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -1): unexpected result: %d\n", bes);
    failed();
  }
  else {
    tprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -1): %d\n", bes);
  }

  /* must be failed */
  ret = spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, 0);
  if (ret < 0) {
    if (errno != EINVAL) {
      eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, 0): %s\n", strerror(errno));
      failed();
    }
  }
  else {
    eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, 0): unexpected result: %d\n", ret);
    failed();
  }

  check_failed();

  /* check the function for each BE. */
  for (i = -1; i < bes; i++) {
    int pspes, uspes;
    pspes = spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, i);
    if (pspes < 0) {
      eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, %d): %s\n", i, strerror(errno));
      failed();
    }
    else if (pspes == 0) {
      eprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, %d): unexpected result: %d\n", i, pspes);
      failed();
    }
    else {
      tprintf("spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, %d): %d\n", i, pspes);
    }

    uspes = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, i);
    if (uspes < 0) {
      eprintf("spe_cpu_info_get(SPE_COUNT_USABLE_SPES, %d): %s\n", i, strerror(errno));
      failed();
    }
    else if (uspes > pspes) {
      eprintf("spe_cpu_info_get(SPE_COUNT_USABLE_SPES, %d): unexpected result: %d\n", i, uspes);
      failed();
    }
    else {
      tprintf("spe_cpu_info_get(SPE_COUNT_USABLE_SPES, %d): %d\n", i, uspes);
    }
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
