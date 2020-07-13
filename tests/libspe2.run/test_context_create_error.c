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

/* This test checks if the library can handle errors regarding
 * creating SPE contexts and SPE gang contexts.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include "ppu_libspe2_test.h"

#define DUMMY_FD_COUNT 100
#define DUMMY_FILE "/dev/null"

static int check_open(int num)
{
  int i;
  int check_fd[DUMMY_FD_COUNT];

  for (i = 0; i < num; i++) {
    check_fd[i] = open(DUMMY_FILE, O_RDWR);
    if (check_fd[i] == -1) {
      if (errno == EMFILE) {
	eprintf("open: Resource (file descriptors) leak in spe_context_create\n");
      }
      else {
	eprintf("open(" DUMMY_FILE "): %s\n", strerror(errno));
      }
      return 1;
    }
  }
  for (i = 0; i < num; i++) {
    close(check_fd[i]);
  }

  return 0;
}

static int test(int argc, char **argv)
{
  spe_context_ptr_t spe;
  spe_gang_context_ptr_t gang;
  int i;

  /* check file descriptors leaks */
  /* spe_context_t */
  for (i = 0; i <= DUMMY_FD_COUNT; i++) {
    if (dummy_file_open_r(i) == -1) {
      fatal();
    }

    spe = spe_context_create(SPE_MAP_PS, NULL);
    if (spe) {
      spe_context_destroy(spe);
      break;
    }

    if (check_open(i)) {
      fatal();
    }

    dummy_file_close();
  }

  /* spe_gang_context_t */
  for (i = 0; i <= DUMMY_FD_COUNT; i++) {
    if (dummy_file_open_r(i) == -1) {
      fatal();
    }

    gang = spe_gang_context_create(0);
    if (gang) {
      spe_gang_context_destroy(gang);
      break;
    }

    if (check_open(i)) {
      fatal();
    }

    dummy_file_close();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
