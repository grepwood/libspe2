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

/* This file provides definitions and declarations used by PPE. */

#ifndef __PPU_LIBSPE2_TEST_H
#define __PPU_LIBSPE2_TEST_H

#include "common_libspe2_test.h"

#include <libspe2.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


/*** main routine of the test ***/
extern int ppu_main(int argc, char **argv, int (*test_proc)(int targc, char **targv));


/*** error handling and messages ***/
/* counter to manage number of errors */
extern int g_failed;
/* increment the # of errors */
#define failed() (++g_failed, eprintf("Error.\n"))
/* report error and exit immediately */
#define fatal() do { failed(); check_failed(); } while (0)
/* check whether any errors have occured and exit immediately if any. */
#define check_failed() do { if (g_failed) { exit(1); } } while (0)

/* check if the content of spe_stop_info_t structure has an expected
   value */
#define check_stop_info(stop_info, expected) \
  check_stop_info_helper(__FILE__, __LINE__, stop_info, expected)
extern int check_stop_info_helper(const char *filename, int line,
				  const spe_stop_info_t *stop_info,
				  const spe_stop_info_t *expected);
/* check if the stop reason is SPE_EXIT and the exit code is an
   expected value. */
#define check_exit_code(stop_info, expected) \
  check_exit_code_helper(__FILE__, __LINE__, stop_info, expected)
extern int check_exit_code_helper(const char *filename, int line,
				  const spe_stop_info_t *stop_info,
				  int expected);

/*
 * Translating the beginning of the LS as spe_error_info_t structure,
 * and displays the contents of the structure. This function is
 * intended to be used in order to print errors on SPE when SPE's
 * printf is unavailable or unreliable, e.g. while testing printf.
 */
extern void print_spe_error_info(spe_context_ptr_t spe);

/* print messages */
#define lmessage(f, n, ...) (fprintf(stderr, "%s:%d: ", f, n), fprintf(stderr, __VA_ARGS__))
#define message(...) lmessage(__FILE__, __LINE__, __VA_ARGS__)
#define leprintf(...) lmessage(__VA_ARGS__)
#define eprintf(...) message(__VA_ARGS__)
#ifdef ENABLE_VERBOSE
#define ltprintf(...) lmessage(__VA_ARGS__)
#define tprintf(...) message(__VA_ARGS__)
#else
#define ltprintf(...)
#define tprintf(...)
#endif

/* convert enum values into human readable strings */
extern const char *mbox_behavior_symbol(unsigned int behavior);
extern const char *proxy_dma_behavior_symbol(unsigned int behavior);
extern const char *stop_reason_symbol(unsigned int reason);
extern const char *runtime_error_symbol(unsigned int error);
extern const char *runtime_exception_symbol(unsigned int exception);
extern const char *ps_area_symbol(enum ps_area ps);


/*** file operations ***/
/* waste file descriptor resource */
extern int dummy_file_open(int num);
extern int dummy_file_open_r(int num);
extern void dummy_file_close(void);

/* generate a unique temporary filename and open it. */
extern int tmpfile_open(char *name /* at leas 256 bytes */, const char *base);

/* SPUFS direct access.
   This function is provided only for test purpose.
   It must *NOT* be used by applications.
 */
extern int spufs_open(const spe_context_ptr_t spe, const char *name, int mode);


/*** test data ***/
/* returns a 32-bit random data */
extern unsigned int rand32(void);
/* fill buffer with test data
 *
 * index and size must be a product of 4.
 */
extern void generate_data(void *ptr, unsigned int idx, size_t size);


/*** time base ***/
extern unsigned int get_timebase_frequency(void);


/*** barrier ***/
typedef struct barrier
{
  int counter;
  int done;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} barrier_t;

#define BARRIER_INITIALIZER {			\
    .counter = 0, .done = 0,			\
      .mutex = PTHREAD_MUTEX_INITIALIZER,	\
      .cond = PTHREAD_COND_INITIALIZER,		\
      }

extern void barrier_initialize(barrier_t *counter);
extern void barrier_finalize(barrier_t *counter);
extern void barrier_sync(barrier_t *counter, int thread_count);

extern void global_sync(int thread_count);


/*** multi-threading helpers ***/
typedef struct ppe_thread ppe_thread_t;
typedef struct ppe_thread_group
{
  ppe_thread_t *threads;
  int nr_threads;
  void *data; /* application data */
} ppe_thread_group_t;
struct ppe_thread
{
  int index;
  pthread_t thread;
  ppe_thread_group_t *group;
};
/* non-blocking I/F */
extern ppe_thread_group_t *
ppe_thread_group_create(size_t num_threads, void *(*proc)(void *), void *data);
extern int ppe_thread_group_wait(ppe_thread_group_t *group, void **exit_codes);
/* blocking I/F */
extern int ppe_thread_group_run(size_t num_threads, void *(*proc)(void *), void *data,
				void **exit_codes);

#endif /* __PPU_LIBSPE2_TEST_H */
