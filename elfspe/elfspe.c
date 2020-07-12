/*
 * elfspe - A wrapper to allow direct execution of SPE binaries
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software Foundation,
 *   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _XOPEN_SOURCE 600
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "libspe2.h"

#ifndef LS_SIZE
#define LS_SIZE                       0x40000   /* 256K (in bytes) */
#endif /* LS_SIZE */

static struct spe_context *ctx;

static void handler (int signr ) __attribute__ ((noreturn));
static void
handler (int signr)
{
  if (ctx)
    spe_context_destroy(ctx);

  fprintf (stderr, "Killed by signal %d\n", signr);
  exit (128 + signr);
}

struct spe_regs {
    unsigned int r3[4];	
    unsigned int r4[4];
    unsigned int r5[4];
};

/* spe_copy_argv - setup C99-style argv[] region for SPE main().
 *
 * Duplicate command line arguments and set up argv pointers 
 * to be absolute LS offsets, allowing SPE program to directly 
 * reference these strings from its own LS.
 *
 * An SPE entry function (crt0.o) can take the data from spe_regs 
 * and copy the argv region from EA to the top of LS (largest addr), 
 * and set $SP per ABI requirements.  Currently, this looks something
 * like:
 *
 *   +-----+ 0
 *   | txt |
 *   |-----|
 *   |     |
 *   |  ^  |
 *   |  |  |
 *   |  |  |
 *   |stack|
 *   |-----| ls_dst (LS_SIZE-nbytes)
 *   | args|
 *   +-----+ LS_SIZE
 */
static int
spe_copy_argv(int argc, char **argv, struct spe_regs *ret)
{
    unsigned int *argv_offsets;
    void *start = NULL;
    char *ptr, *end;
    union {
	unsigned long long ull;
	unsigned int ui[2];
    } addr64;
    int spe_arg_max = 4096;
    int i, nbytes = 0;

    memset(ret, 0, sizeof(struct spe_regs));
    if ((argc <= 0) || getenv("SPE_NO_ARGS")) {
	return 0;
    }
    if (getenv("SPE_ARG_MAX")) {
	int v;
	v = strtol((const char *) getenv("SPE_ARG_MAX"), (char **) NULL,
		   0);
	if ((v >= 0) && (v <= spe_arg_max))
	    spe_arg_max = v & ~(15);
    }
    for (i = 0; i < argc; i++) {
	nbytes += strlen(argv[i]) + 1;
    }
    nbytes += (argc * sizeof(unsigned int));
    nbytes = (nbytes + 15) & ~(15);
    if (nbytes > spe_arg_max) {
	return 2;
    }
    posix_memalign(&start, 16, nbytes);
    if (!start) {
	return 3;
    }
    addr64.ull = (unsigned long long) ((unsigned long) start);
    memset(start, 0, nbytes);
    ptr = (char *)start;
    end = (char *)start + nbytes;
    argv_offsets = (unsigned int *) ptr;
    ptr += (argc * sizeof(unsigned int));
    for (i = 0; i < argc; i++) {
	int len = strlen(argv[i]) + 1;
	argv_offsets[i] = LS_SIZE - ((unsigned int) (end - ptr));
	argv_offsets[i] &= LS_SIZE-1;
	memcpy(ptr, argv[i], len);
	ptr += len;
    }
    ret->r3[0] = argc;
    ret->r4[0] = LS_SIZE - nbytes;
    ret->r4[1] = addr64.ui[0];
    ret->r4[2] = addr64.ui[1];
    ret->r4[3] = nbytes;
    return 0;
}

int
main (int argc, char **argv)
{
  int flags = 0;
  unsigned int entry = SPE_DEFAULT_ENTRY;

  struct spe_regs params;
  int rc;
  spe_stop_info_t stop_info;
  spe_program_handle_t *spe_handle;

  signal (SIGSEGV, handler);
  signal (SIGBUS, handler);
  signal (SIGILL, handler);
  signal (SIGFPE, handler);
  signal (SIGTERM, handler);
  signal (SIGHUP, handler);
  signal (SIGINT, handler);

  if (argc < 2) {
      fprintf (stderr, "Usage: elfspe [spe-elf]\n");
      exit (1);
  }

  spe_handle = spe_image_open (argv[1]);
  if (!spe_handle) {
      perror (argv[1]);
      exit (1);
  }

  if (spe_copy_argv(argc-1, &argv[1], &params)) {
	perror ("spe_copy_argv");
	exit (1);
  }
	
  ctx = spe_context_create(flags, NULL);
  if (ctx == NULL) {
    perror("spe_create_single");
    exit(1);
  }
  
  if (spe_program_load(ctx, spe_handle)) {
    perror("spe_load");
    exit(1);
  }
   
  rc = spe_context_run(ctx, &entry, SPE_RUN_USER_REGS, &params, NULL, &stop_info);
  if (rc < 0)
    perror("spe_run");
	
  spe_context_destroy(ctx);

  return stop_info.result.spe_exit_code;
}

