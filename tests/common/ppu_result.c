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

#include <string.h>

#include "ppu_libspe2_test.h"


/*** checking results and/or printing information ***/
int check_stop_info_helper(const char *filename, int line,
			   const spe_stop_info_t *stop_info,
			   const spe_stop_info_t *expected)
{
  if (stop_info->stop_reason != expected->stop_reason) {
    leprintf(filename, line,
	     "Unexpected reason: %s (%d)\n",
	     stop_reason_symbol(stop_info->stop_reason), stop_info->stop_reason);
    return 1;
  }

  switch (stop_info->stop_reason) {
  case SPE_EXIT:
    if (stop_info->result.spe_exit_code != expected->result.spe_exit_code) {
      leprintf(filename, line,
	       "Unexpected exit code: %u (0x%x)\n",
	       stop_info->result.spe_exit_code, stop_info->result.spe_exit_code);
      return 1;
    }
    break;
  case SPE_STOP_AND_SIGNAL:
    if (stop_info->result.spe_signal_code != expected->result.spe_signal_code) {
      leprintf(filename, line,
	       "Unexpected signal code: 0x%x\n", stop_info->result.spe_signal_code);
      return 1;
    }
    break;
  case SPE_RUNTIME_ERROR:
    if (stop_info->result.spe_runtime_error != expected->result.spe_runtime_error) {
      leprintf(filename, line,
	       "Unexpected runtime error: 0x%x (%s)\n",
	       stop_info->result.spe_runtime_error,
	       runtime_error_symbol(stop_info->result.spe_runtime_error));
      return 1;
    }
    break;
  case SPE_RUNTIME_FATAL:
    if (stop_info->result.spe_runtime_fatal != expected->result.spe_runtime_fatal) {
      leprintf(filename, line,
	       "Unexpected errno: %d (%s)\n",
	       stop_info->result.spe_runtime_fatal,
	       strerror(stop_info->result.spe_runtime_fatal));
      return 1;
    }
    break;
  case SPE_RUNTIME_EXCEPTION:
    if (stop_info->result.spe_runtime_exception != expected->result.spe_runtime_exception) {
      leprintf(filename, line,
	       "Unexpected runtime exception: 0x%x (%s)\n",
	       stop_info->result.spe_runtime_exception,
	       runtime_exception_symbol(stop_info->result.spe_runtime_exception));
      return 1;
    }
    break;
  case SPE_CALLBACK_ERROR:
    if (stop_info->result.spe_callback_error != expected->result.spe_callback_error) {
      leprintf(filename, line,
	       "Unexpected callback error: 0x%x\n", stop_info->result.spe_callback_error);
      return 1;
    }
    break;
  case SPE_ISOLATION_ERROR:
    if (stop_info->result.spe_isolation_error != expected->result.spe_isolation_error) {
      leprintf(filename, line,
	       "Unexpected isolation error: 0x%x\n", stop_info->result.spe_isolation_error);
      return 1;
    }
    break;
  default:
    leprintf(filename, line,
	     "Unknown stop reason: %d\n", stop_info->stop_reason);
    return 1;
  }

  return 0;
}

int check_exit_code_helper(const char *filename, int line,
			   const spe_stop_info_t *stop_info, int expected_code)
{
  spe_stop_info_t expected;

  expected.stop_reason = SPE_EXIT;
  expected.result.spe_exit_code = expected_code;

  return check_stop_info_helper(filename, line, stop_info, &expected);
}

void print_spe_error_info(spe_context_ptr_t spe)
{
  const void *ls = spe_ls_area_get(spe);
  const spe_error_info_t *err = (const spe_error_info_t *)ls;
  if (!err) {
    tprintf("Could not get LS address. Ignored.\n");
    return;
  }
  fprintf(stderr, "%s:%d: Error in SPE. exit code == %d, errno == %d.\n",
	  (const char *)ls + err->file, err->line, err->code, err->err);
}

/*** get human readable strings corresponding to symbols ***/
const char *mbox_behavior_symbol(unsigned int behavior)
{
  switch (behavior) {
  case SPE_MBOX_ALL_BLOCKING:
    return "SPE_MBOX_ALL_BLOCKING";
  case SPE_MBOX_ANY_BLOCKING:
    return "SPE_MBOX_ANY_BLOCKING";
  case SPE_MBOX_ANY_NONBLOCKING:
    return "SPE_MBOX_ANY_NONBLOCKING";
  default:
    return "unknown";
  }
}

const char *proxy_dma_behavior_symbol(unsigned int behavior)
{
  switch (behavior) {
  case SPE_TAG_ANY:
    return "SPE_TAG_ANY";
  case SPE_TAG_ALL:
    return "SPE_TAG_ALL";
  case SPE_TAG_IMMEDIATE:
    return "SPE_TAG_IMMEDIATE";
  default:
    return "unknown";
  }
}

const char *stop_reason_symbol(unsigned int reason)
{
  switch (reason) {
  case SPE_EXIT:
    return "SPE_EXIT";
  case SPE_STOP_AND_SIGNAL:
    return "SPE_STOP_AND_SIGNAL";
  case SPE_RUNTIME_ERROR:
    return "SPE_RUNTIME_ERROR";
  case SPE_RUNTIME_EXCEPTION:
    return "SPE_RUNTIME_EXCEPTION";
  case SPE_RUNTIME_FATAL:
    return "SPE_RUNTIME_FATAL";
  case SPE_CALLBACK_ERROR:
    return "SPE_CALLBACK_ERROR";
  case SPE_ISOLATION_ERROR:
    return "SPE_ISOLATION_ERROR";
  default:
    return "unknown";
  }
}

const char *runtime_error_symbol(unsigned int error)
{
  switch (error) {
  case SPE_SPU_STOPPED_BY_STOP: /* INTERNAL USE ONLY */
    return "SPE_SPU_STOPPED_BY_STOP";
  case SPE_SPU_HALT:
    return "SPE_SPU_HALT";
  case SPE_SPU_WAITING_ON_CHANNEL: /* INTERNAL USE ONLY */
    return "SPE_SPU_WAITING_ON_CHANNEL";
  case SPE_SPU_SINGLE_STEP:
    return "SPE_SPU_SINGLE_STEP";
  case SPE_SPU_INVALID_INSTR:
    return "SPE_SPU_INVALID_INSTR";
  case SPE_SPU_INVALID_CHANNEL:
    return "SPE_SPU_INVALID_CHANNEL";
  default:
    return "unknown";
  }
}

const char *runtime_exception_symbol(unsigned int exception)
{
  switch (exception) {
  case SPE_DMA_ALIGNMENT:
    return "SPE_DMA_ALIGNMENT";
  case SPE_DMA_SEGMENTATION:
    return "SPE_DMA_SEGMENTATION";
  case SPE_DMA_STORAGE:
    return "SPE_DMA_STORAGE";
  case SPE_INVALID_DMA:
    return "SPE_INVALID_DMA";
  default:
    return "unknown";
  }
}

const char *ps_area_symbol(enum ps_area ps)
{
  switch (ps) {
  case SPE_MSSYNC_AREA:
    return "SPE_MSSYNC_AREA";
  case SPE_MFC_COMMAND_AREA:
    return "SPE_MFC_COMMAND_AREA";
  case SPE_CONTROL_AREA:
    return "SPE_CONTROL_AREA";
  case SPE_SIG_NOTIFY_1_AREA:
    return "SPE_SIG_NOTIFY_1_AREA";
  case SPE_SIG_NOTIFY_2_AREA:
    return "SPE_SIG_NOTIFY_2_AREA";
  default:
    return "unknown";
  }
}
