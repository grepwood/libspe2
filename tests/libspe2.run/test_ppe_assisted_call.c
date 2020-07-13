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

/* This test checks if the PPE assisted library call support works
 * correctly.
 */

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define ERROR_VALUE 0xdeadbeaf
#define CALL_NUM (PPE_ASSISTED_CALL_SIGNAL_TYPE - 0x2100)

extern spe_program_handle_t spu_ppe_assisted_call;

static int run(unsigned int call_num,
	       int (*check_func)(int ret, spe_stop_info_t *stop_info))
{
  int ret;
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;

  spe = spe_context_create(0, NULL);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    return 1;
  }

  if (spe_program_load(spe, &spu_ppe_assisted_call)) {
    eprintf("spe_program_load(%p, &spu_ppe_assisted_call): %s\n", spe, strerror(errno));
    return 1;
  }

  ret = spe_context_run(spe, &entry, 0, (void*)call_num, NULL, &stop_info);
  ret = (*check_func)(ret, &stop_info);
  if (ret) {
    return 1;
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    return 1;
  }

  return 0;
}

static int handler_error(void *ls_base, unsigned int ls_address)
{
  return ERROR_VALUE;
}

static int handler_argument(void *ls_base, unsigned int ls_address)
{
  unsigned int *args = (unsigned int *)(ls_base + ls_address);

  if (*args != PPE_ASSISTED_CALL_DATA) {
    eprintf("assisted call: Argument mismatch: 0x%08x\n", *args);
    return 1;
  }

  return 0;
}

static int check_error_value(int ret, spe_stop_info_t *stop_info)
{
  spe_stop_info_t expected;

  if (ret == 0) {
    eprintf("spe_context_run: Unexpected success.\n");
    return 1;
  }
  if (errno != EFAULT) {
    eprintf("spe_context_run: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    return 1;
  }
  expected.stop_reason = SPE_CALLBACK_ERROR;
  expected.result.spe_callback_error = ERROR_VALUE;
  if (check_stop_info(stop_info, &expected)) {
    return 1;
  }
  return 0;
}

static int check_argument(int ret, spe_stop_info_t *stop_info)
{
  if (ret != 0) {
    eprintf("spe_context_run: %s\n", strerror(errno));
    return 1;
  }
  return 0;
}

static int test_invalid(int call_num)
{
  int ret;
  void *query;

  query = spe_callback_handler_query(call_num);
  if (query) {
    eprintf("spe_callback_handler_query: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != EINVAL) {
    eprintf("spe_callback_handler_query: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  ret = spe_callback_handler_register(handler_error, call_num, SPE_CALLBACK_NEW);
  if (ret == 0) {
    eprintf("spe_callback_handler_register: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != EINVAL) {
    eprintf("spe_callback_handler_register: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  ret = spe_callback_handler_deregister(call_num);
  if (ret == 0) {
    eprintf("spe_callback_handler_deregister: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != EINVAL) {
    eprintf("spe_callback_handler_deregister: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  return 0;
}

static int test_used(int call_num)
{
  int ret;
  void *query;

  /* query on registered call num */
  query = spe_callback_handler_query(call_num);
  if (!query) {
    eprintf("spe_callback_handler_query: %d: Unexpected failure: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  /* register as used call num */
  ret = spe_callback_handler_register(handler_error, call_num, SPE_CALLBACK_NEW);
  if (ret == 0) {
    eprintf("spe_callback_handler_register: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != EACCES) {
    eprintf("spe_callback_handler_register: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  return 0;
}

static int test_unused(int call_num)
{
  int ret;
  void *query;

  /* query on unregistered call num */
  query = spe_callback_handler_query(call_num);
  if (query) {
    eprintf("spe_callback_handler_query: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != ESRCH) {
    eprintf("spe_callback_handler_query: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  /* update unused call num */
  ret = spe_callback_handler_register(handler_error, call_num, SPE_CALLBACK_UPDATE);
  if (ret == 0) {
    eprintf("spe_callback_handler_register: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != ESRCH) {
    eprintf("spe_callback_handler_register: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  /* make sure that 'handler_error' is not registered */
  query = spe_callback_handler_query(call_num);
  if (query) {
    eprintf("spe_callback_handler_query: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno != ESRCH) {
    eprintf("spe_callback_handler_query: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  /* unregister unused handler */
  ret = spe_callback_handler_deregister(call_num);
  if (ret == 0) {
    eprintf("spe_callback_handler_deregister: %d: Unexpected success.\n", call_num);
    return -1;
  }
  if (errno == EACCES) { /* reserved call num */
    return 0;
  }
  if (errno != ESRCH) {
    eprintf("spe_callback_handler_deregister: %d: Unexpected errno: %d (%s)\n",
	    call_num, errno, strerror(errno));
    return -1;
  }

  /*** normal conditions ***/
  ret = spe_callback_handler_register(handler_error, call_num, SPE_CALLBACK_NEW);
  if (ret) {
    eprintf("spe_callback_handler_register(handler_error, %u, SPE_CALLBACK_NEW): %s\n",
	    call_num, strerror(errno));
    return -1;
  }

  query = spe_callback_handler_query(call_num);
  if (!query) {
    eprintf("spe_callback_handler_query: %d: %s\n", call_num, strerror(errno));
    return -1;
  }
  if (query != handler_error) {
    eprintf("spe_callback_handler_query: %d: Unexpected result %p\n", call_num, query);
    return -1;
  }

  ret = run(call_num, check_error_value);
  if (ret) {
    return -1;
  }

  ret = spe_callback_handler_register(handler_argument, call_num, SPE_CALLBACK_UPDATE);
  if (ret) {
    eprintf("spe_callback_handler_register: %d: %s\n", call_num, strerror(errno));
    return -1;
  }

  ret = run(call_num, check_argument);
  if (ret) {
    return -1;
  }

  ret = spe_callback_handler_deregister(call_num);
  if (ret) {
    eprintf("spe_callback_handler_deregister: %d: %s\n", call_num, strerror(errno));
    return -1;
  }

  return 0;
}

static int test(int argc, char **argv)
{
  int ret;
  int i;

  /* invalid handler */
  ret = spe_callback_handler_register(NULL, CALL_NUM, SPE_CALLBACK_NEW);
  if (ret == 0) {
    eprintf("spe_callback_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_callback_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* invalid mode */
  ret = spe_callback_handler_register(handler_error, CALL_NUM, -1);
  if (ret == 0) {
    eprintf("spe_callback_handler_register: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_callback_handler_register: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* walk through all call num */
  for (i = -1; i <= 256; i++) {
    void *query;

    switch (i) {
    case -1:
    case 256:
      ret = test_invalid(i);
      break;
    case 0:
      ret = test_used(i);
      break;
    case CALL_NUM:
      ret = test_unused(i);
      break;
    case 4:
      continue; /* handled by the kernel */
    default:
      query = spe_callback_handler_query(i);
      ret = query ? test_used(i) : test_unused(i);
      break;
    }

    if (ret) {
      fatal();
    }
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
