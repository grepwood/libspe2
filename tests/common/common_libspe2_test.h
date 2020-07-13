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

/* This file provides common definitions and declarations shared
 * between PPE and SPEs.
 */

#ifndef __COMMON_LIBSPE2_TEST_H
#define __COMMON_LIBSPE2_TEST_H

#include "config_libspe2_test.h"

#include <assert.h>

/*** configurations ***/
#define ENABLE_TEST_EVENT_OUT_INTR_MBOX 1
#define ENABLE_TEST_EVENT_SPE_STOPPED   1
//#define ENABLE_TEST_EVENT_TAG_GROUP 1 /* this test currently hits SPUFS's bug */


/*** test parameters ***/
#ifndef NUM_SPES
#define NUM_SPES 4
#endif

#ifndef WBOX_DEPTH
#define WBOX_DEPTH 4
#endif

#ifndef MBOX_DEPTH
#define MBOX_DEPTH 1
#endif

#ifndef IBOX_DEPTH
#define IBOX_DEPTH 1
#endif

#ifndef MFC_COMMAND_QUEUE_DEPTH
#define MFC_COMMAND_QUEUE_DEPTH 16
#endif

#ifndef MFC_PROXY_COMMAND_QUEUE_DEPTH
#define MFC_PROXY_COMMAND_QUEUE_DEPTH 8
#endif

#ifndef MAX_DMA_SIZE
#define MAX_DMA_SIZE (1024 * 16)
#endif

#ifndef NUM_ITERATIONS
#define NUM_ITERATIONS 1
#endif

/*** test data ***/
#define RUN_ARGP_DATA 0x12345678
#define RUN_ENVP_DATA 0x9abcdef0

#define EXIT_DATA     107

#define STOP_DATA     1000

#define PPE_ASSISTED_CALL_SIGNAL_TYPE 0x21ff
#define PPE_ASSISTED_CALL_DATA 1234

#define SPE_INVALID_INSTR_OPCODE 0x800000
#define MFC_INVALID_COMMAND_OPCODE 0x7fff

#define MAX_MBOX_COUNT  WBOX_DEPTH


/*** limits ***/
#define MAX_DUMMY_FDS 10000


/*** utilities ***/
#define ASSERT(e) assert(e)

#define STRING(a) _STR(a)
#define WCHAR(t) _CAT(L, t)

#define _STR(a) # a
#define _CAT(a, b) a ## b

/*** structure to be used to record information about error in SPE
 * test code. In the C99 handler's test cases, we can't use SPE's
 * 'printf' function to report errors because a part of the function
 * is implemented in the C99 handler itself. :-) So we report errors
 * via LS by using this structure in such cases.
 ***/
typedef struct spe_error_info
{
  unsigned int code;
  unsigned int file;
  unsigned int line;
  unsigned int err; /* errno */
} spe_error_info_t;


#endif /* __COMMON_LIBSPE2_TEST_H */
