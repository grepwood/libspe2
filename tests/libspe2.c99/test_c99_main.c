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

/* This program is a helper to run SPE programs for default C99
 * handler tests. This program:
 *
 *     - runs a SPE program specified in its command line.
 *
 *     - checks result code returned by the SPE program.
 *
 *     - displays contents of an spe_error_info_t structure located at
 *       the beginning of LS, if the SPE program returns error.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

int main(int argc, char **argv)
{
  spe_context_ptr_t spe;
  spe_program_handle_t *prog;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  char *elf_filename;
  spe_stop_info_t stop_info;

  if (argc <= 1) {
    eprintf("No SPE ELF file is specified.\n");
    fatal();
  }

  elf_filename = argv[1];

  prog = spe_image_open(elf_filename);
  if (!prog) {
    eprintf("spe_image_open(\"%s\"): %s\n", elf_filename, strerror(errno));
    fatal();
  }

  spe = spe_context_create(0, NULL);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, prog)) {
    eprintf("spe_program_load(%p, %p): %s\n", spe, prog, strerror(errno));
    fatal();
  }

  ret = spe_context_run(spe, &entry, 0, NULL, NULL, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      print_spe_error_info(spe);
      fatal();
    }
  }
  else {
    eprintf("spe_context_run: %s\n", strerror(errno));
    fatal();
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }

  spe_image_close(prog);

  return 0;
}
