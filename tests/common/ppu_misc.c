/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2008 Sony Computer Entertainment Inc.
 *  Copyright 2008 Sony Corp.
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

#include "ppu_libspe2_test.h"

void generate_data(void *data, unsigned int idx, size_t size)
{
  /* index and size must be a product of 4 */
  unsigned int *ptr = data;
  unsigned int *end = data + size;
  unsigned int value = idx / 4;

  while (ptr < end) {
    *ptr++ = value++;
  }
}

unsigned int rand32(void)
{
  return rand() ^ (rand() << 16);
}

unsigned int get_timebase_frequency(void)
{
  const char *filename = "/proc/cpuinfo";
  char buf[256];
  FILE *fp;

  fp = fopen(filename, "r");
  if (!fp) {
    return -1;
  }
  while (fgets(buf, sizeof(buf), fp)) {
    unsigned long long tb;
    if (sscanf(buf, "timebase%*[ \t]:%llu\n", &tb) == 1) {
      fclose(fp);
      return tb;
    }
  }
  fclose(fp);
  return 0;
}
