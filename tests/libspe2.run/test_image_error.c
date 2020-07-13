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

/* This test checks if the library can handle errors in the
 * spe_image_* functions correctly.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define COUNT 1000

#define NON_EXEC_FILE "spu_non_exec.spu.elf" /* SPU ELF, no permission */
#define NON_ELF_FILE "spu_non_elf.spu.elf" /* non-ELF format */
#define NON_SPU_ELF_FILE argv[0] /* PPU ELF */

int main(int argc, char **argv)
{
  spe_program_handle_t *prog;
  int i;
  int avail_fds, num_fds;
  int ret;

  /* count # of available file descriptors */
  avail_fds = dummy_file_open_r(0);
  if (avail_fds == -1) {
    eprintf("dummy_file_open_r(0): %s\n", strerror(errno));
    fatal();
  }
  dummy_file_close();


  /*** try non-executable file ***/
  for (i = 0; i < COUNT; i++) {
    prog = spe_image_open(NON_EXEC_FILE);
    if (prog) {
      eprintf("spe_image_open(%s): Unexpected success.\n", NON_EXEC_FILE);
      fatal();
    }
    if (errno != EACCES) {
      eprintf("spe_image_open(%s): Unexpected errno: %d (%s)\n",
	      NON_EXEC_FILE, errno, strerror(errno));
      fatal();
    }
  }
  /* check FD leaks */
  num_fds = dummy_file_open_r(0);
  if (num_fds == -1) {
    eprintf("dummy_file_open_r(0): %s\n", strerror(errno));
    fatal();
  }
  dummy_file_close();
  if (num_fds != avail_fds) {
    eprintf("spe_image_open(%s): FD leaks.\n", NON_EXEC_FILE);
    fatal();
  }


  /*** try non-ELF file ***/
  for (i = 0; i < COUNT; i++) {
    prog = spe_image_open(NON_ELF_FILE);
    if (prog) {
      eprintf("spe_image_open(%s): Unexpected success.\n", NON_ELF_FILE);
      fatal();
    }
    if (errno != EINVAL) {
      eprintf("spe_image_open(%s): Unexpected errno: %d (%s)\n",
	      NON_ELF_FILE, errno, strerror(errno));
      fatal();
    }
  }
  /* check FD leaks */
  num_fds = dummy_file_open_r(0);
  if (num_fds == -1) {
    eprintf("dummy_file_open_r(0): %s\n", strerror(errno));
    fatal();
  }
  dummy_file_close();
  if (num_fds != avail_fds) {
    eprintf("spe_image_open(%s): FD leaks.\n", NON_ELF_FILE);
    fatal();
  }


  /*** try non-SPU-ELF file ***/
  for (i = 0; i < COUNT; i++) {
    prog = spe_image_open(NON_SPU_ELF_FILE);
    if (prog) {
      eprintf("spe_image_open(%s): Unexpected success.\n", NON_SPU_ELF_FILE);
      fatal();
    }
    if (errno != EINVAL) {
      eprintf("spe_image_open(%s): Unexpected errno: %d (%s)\n",
	      NON_SPU_ELF_FILE, errno, strerror(errno));
      fatal();
    }
  }
  /* check FD leaks */
  num_fds = dummy_file_open_r(0);
  if (num_fds == -1) {
    eprintf("dummy_file_open_r(0): %s\n", strerror(errno));
    fatal();
  }
  dummy_file_close();
  if (num_fds != avail_fds) {
    eprintf("spe_image_open(%s): FD leaks.\n", NON_SPU_ELF_FILE);
    fatal();
  }


  /* try invalid program handle */
  ret = spe_image_close(NULL);
  if (ret == 0) {
    eprintf("spe_image_close(NULL): Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_image_close(NULL): Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    fatal();
  }

  return 0;
}
