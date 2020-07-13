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

/* This test checks if one SPE context can be created and be run
 * correctly.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

extern spe_program_handle_t spu_exit;
extern spe_program_handle_t spu_arg;

static int run(spe_program_handle_t *prog, int reg_f, void *argp, void *envp, int exit_code)
{
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  spe = spe_context_create(0, NULL);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    return 1;
  }

  if (spe_program_load(spe, prog)) {
    eprintf("spe_program_load(%p, %p): %s\n", spe, prog, strerror(errno));
    return 1;
  }

  if (reg_f) {
    unsigned long long regs[3][2];
    regs[0][0] = (unsigned long)spe;
    regs[1][0] = (unsigned long)argp;
    regs[2][0] = (unsigned long)envp;
    ret = spe_context_run(spe, &entry, SPE_RUN_USER_REGS, regs, NULL, &stop_info);
  }
  else {
    ret = spe_context_run(spe, &entry, 0, argp, envp, &stop_info);
  }
  if (ret == 0) {
    if (check_exit_code(&stop_info, exit_code)) {
      return 1;
    }
  }
  else {
    eprintf("spe_context_run(%p, ...): %s\n", spe, strerror(errno));
    return 1;
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    return 1;
  }

  return 0;
}

static int test(int argc, char **argv)
{
  int ret;

  /* exit code test */
  ret = run(&spu_exit, 0, 0, 0, EXIT_DATA);
  if (ret) fatal();

  /* argument test */
  ret = run(&spu_arg, 0, (void*)RUN_ARGP_DATA, (void*)RUN_ENVP_DATA, 0);
  if (ret) fatal();

  /* argument test (user regs) */
  ret = run(&spu_arg, 1, (void*)RUN_ARGP_DATA, (void*)RUN_ENVP_DATA, 0);
  if (ret) fatal();

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
