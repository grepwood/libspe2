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

/* This test checks if the library can handle errors in the
 * spe_context_run function correctly.
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <sys/mman.h>

#include "ppu_libspe2_test.h"

/* The definition of DMA segment error has been removed from the kernel,
   and the error should be reported as a DMA storage error. */
// #define ENABLE_TEST_DMA_SEMGENT_ERROR 1

/* The current Cell/B.E. hardware implementation never raises INVALID
   CHANNEL error. */
// #define ENABLE_TEST_INVALID_CHANNEL_ERROR 1


extern spe_program_handle_t spu_invalid_instr;
extern spe_program_handle_t spu_invalid_channel;
extern spe_program_handle_t spu_halt;
extern spe_program_handle_t spu_dma_error;
extern spe_program_handle_t spu_invalid_dma;

static int g_illegal_instr_f;
static int g_bus_error_f;
static int g_segv_error_f;

static unsigned char g_dma_buffer[MAX_DMA_SIZE] __attribute__((aligned(16)));


static int run(spe_program_handle_t *prog, unsigned int create_flags,
	       void *run_argp, void *run_envp,
	       int (*check_func)(int ret, spe_stop_info_t *stop_info))
{
  int ret, result;
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;

  g_illegal_instr_f = 0;
  g_bus_error_f = 0;
  g_segv_error_f = 0;

  spe = spe_context_create(create_flags, NULL);
  if (!spe) {
    eprintf("spe_context_create(0x%08x, NULL): %s\n", create_flags, strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, prog)) {
    eprintf("spe_program_load(%p, %p): %s\n", spe, prog, strerror(errno));
    fatal();
  }

  ret = spe_context_run(spe, &entry, 0, run_argp, run_envp, &stop_info);
  result = (*check_func)(ret, &stop_info);

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }

  return result;
}

static int check_run_result(int ret,
			    const spe_stop_info_t *stop_info,
			    int expected_ret,
			    const spe_stop_info_t *expected)
{

  if (ret != expected_ret) {
    eprintf("Unexpected return value: %d\n", ret);
    return 1;
  }

  return check_stop_info(stop_info, expected);
}

static int check_invalid_instr_signal(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_illegal_instr_f == 0) {
    eprintf("No SIGILL was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_FATAL;
  expected.result.spe_runtime_fatal = EINTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_dma_alignment_signal(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (!g_bus_error_f) {
    eprintf("No SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_FATAL;
  expected.result.spe_runtime_fatal = EINTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

#ifdef ENABLE_TEST_DMA_SEMGENT_ERROR
static int check_dma_segmentation_signal(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (!g_segv_error_f && !g_bus_error_f) {
    eprintf("No SIGSEGV nor SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_FATAL;
  expected.result.spe_runtime_fatal = EINTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}
#endif

static int check_dma_storage_signal(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (!g_segv_error_f && !g_bus_error_f) {
    eprintf("No SIGSEGV nor SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_FATAL;
  expected.result.spe_runtime_fatal = EINTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_invalid_dma_signal(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (!g_bus_error_f) {
    eprintf("No SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_FATAL;
  expected.result.spe_runtime_fatal = EINTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_invalid_instr_event(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_illegal_instr_f) {
    eprintf("Unexpected SIGILL was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_ERROR;
  expected.result.spe_runtime_error = SPE_SPU_INVALID_INSTR;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_dma_alignment_event(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_bus_error_f) {
    eprintf("Unexpected SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_EXCEPTION;
  expected.result.spe_runtime_exception = SPE_DMA_ALIGNMENT;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

#ifdef ENABLE_TEST_DMA_SEMGENT_ERROR
static int check_dma_segmentation_event(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_segv_error_f || g_bus_error_f) {
    eprintf("Unexpected SIGSEGV or SUGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_EXCEPTION;
  expected.result.spe_runtime_exception = SPE_DMA_SEGMENTATION;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}
#endif

static int check_dma_storage_event(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_segv_error_f || g_bus_error_f) {
    eprintf("Unexpected SIGSEGV or SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_EXCEPTION;
  expected.result.spe_runtime_exception = SPE_DMA_STORAGE;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_invalid_dma_event(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (g_bus_error_f) {
    eprintf("Unexpected SIGBUS was detected.\n");
    return 1;
  }

  expected.stop_reason = SPE_RUNTIME_EXCEPTION;
  expected.result.spe_runtime_exception = SPE_INVALID_DMA;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static int check_halt(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  expected.stop_reason = SPE_RUNTIME_ERROR;
  expected.result.spe_runtime_error = SPE_SPU_HALT;
  if (check_run_result(ret, stop_info, -1, &expected)) {
    return 1;
  }

  return 0;
}

static void sigill_handler(int sig, siginfo_t *siginfo, void *uctx)
{
  g_illegal_instr_f++;
}

static void sigbus_handler(int sig, siginfo_t *siginfo, void *uctx)
{
  g_bus_error_f++;
}

static void sigsegv_handler(int sig, siginfo_t *siginfo, void *uctx)
{
  g_segv_error_f++;
}

static int test(int argc, char **argv)
{
  int ret;
  struct sigaction sa;
  void *ro_dma_buffer;

  ro_dma_buffer = mmap(NULL, MAX_DMA_SIZE, PROT_READ,
		       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (ro_dma_buffer == MAP_FAILED) {
    eprintf("mmap(NULL, ...): %s\n", strerror(errno));
    fatal();
  }

  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = sigill_handler;
  sigaction(SIGILL, &sa, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = sigbus_handler;
  sigaction(SIGBUS, &sa, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = sigsegv_handler;
  sigaction(SIGSEGV, &sa, NULL);

  ret = run(&spu_halt, 0, NULL, NULL, check_halt);
  if (ret) {
    failed();
  }

  ret = run(&spu_invalid_instr, 0, NULL, NULL, check_invalid_instr_signal);
  if (ret) {
    failed();
  }

  ret = run(&spu_dma_error, 0, g_dma_buffer, (void*)1, check_dma_alignment_signal);
  if (ret) {
    failed();
  }

#ifdef ENABLE_TEST_DMA_SEMGENT_ERROR
  ret = run(&spu_dma_error, 0, NULL, NULL, check_dma_segmentation_signal);
  if (ret) {
    failed();
  }
#endif

  ret = run(&spu_dma_error, 0, ro_dma_buffer, NULL, check_dma_storage_signal);
  if (ret) {
    failed();
  }

  ret = run(&spu_invalid_dma, 0, g_dma_buffer, NULL, check_invalid_dma_signal);
  if (ret) {
    failed();
  }

  ret = run(&spu_invalid_instr, SPE_EVENTS_ENABLE, NULL, NULL,
	    check_invalid_instr_event);
  if (ret) {
    failed();
  }

  ret = run(&spu_dma_error, SPE_EVENTS_ENABLE, g_dma_buffer, (void*)1,
	    check_dma_alignment_event);
  if (ret) {
    failed();
  }

#ifdef ENABLE_TEST_DMA_SEMGENT_ERROR
  ret = run(&spu_dma_error, SPE_EVENTS_ENABLE, NULL, NULL,
	    check_dma_segmentation_event);
  if (ret) {
    failed();
  }
#endif

  ret = run(&spu_dma_error, SPE_EVENTS_ENABLE, ro_dma_buffer, NULL,
	    check_dma_storage_event);
  if (ret) {
    failed();
  }

  ret = run(&spu_invalid_dma, SPE_EVENTS_ENABLE, g_dma_buffer, NULL,
	    check_invalid_dma_event);
  if (ret) {
    failed();
  }

#ifdef ENABLE_TEST_INVALID_CHANNEL_ERROR
#  error "Not implemented yet."
#endif

  munmap(ro_dma_buffer, MAX_DMA_SIZE);

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
