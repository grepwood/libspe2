/*
 * libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <errno.h>
#define GNU_SOURCE 1

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <sys/types.h>

#include <sys/spu.h>

#include "elf_loader.h"
#include "lib_builtin.h"
#include "spebase.h"

/*Thread-local variable for use by the debugger*/
__thread struct spe_context_info {
	int spe_id;
	unsigned int npc;
	unsigned int status;
	struct spe_context_info *prev;
}*__spe_current_active_context;


static void cleanupspeinfo(struct spe_context_info *ctxinfo)
{
	struct spe_context_info *tmp = ctxinfo->prev;
	__spe_current_active_context = tmp;
}

static int set_regs(struct spe_context *spe, void *regs)
{
	int fd_regs, rc;

	fd_regs = openat(spe->base_private->fd_spe_dir, "regs", O_RDWR);
	if (fd_regs < 0) {
		DEBUG_PRINTF("Could not open SPE regs file.\n");
		errno = EFAULT;
		return -1;
	}

	rc = write(fd_regs, regs, 2048);

	close(fd_regs);

	if (rc < 0)
		return -1;

	return 0;
}

static int issue_isolated_exit(struct spe_context *spe)
{
	struct spe_spu_control_area *cntl_area =
		spe->base_private->cntl_mmap_base;

	if (!cntl_area) {
		DEBUG_PRINTF("%s: could not access SPE control area\n",
				__FUNCTION__);
		return -1;
	}

	/* 0x2 is an isolated exit request */
	cntl_area->SPU_RunCntl = 0x2;

	return 0;
}

static inline void freespeinfo()
{
	/*Clean up the debug variable*/
	struct spe_context_info *tmp = __spe_current_active_context->prev;
	__spe_current_active_context = tmp;
}

int _base_spe_context_run(spe_context_ptr_t spe, unsigned int *entry,
		unsigned int runflags, void *argp, void *envp,
		spe_stop_info_t *stopinfo)
{
	int retval = 0, run_rc;
	unsigned int run_status, tmp_entry;
	spe_stop_info_t	stopinfo_buf;
	struct spe_context_info this_context_info __attribute__((cleanup(cleanupspeinfo)));

	/* If the caller hasn't set a stopinfo buffer, provide a buffer on the
	 * stack instead. */
	if (!stopinfo)
		stopinfo = &stopinfo_buf;


	/* In emulated isolated mode, the npc will always return as zero.
	 * use our private entry point instead */
	if (spe->base_private->flags & SPE_ISOLATE_EMULATE)
		tmp_entry = spe->base_private->emulated_entry;

	else if (*entry == SPE_DEFAULT_ENTRY)
		tmp_entry = spe->base_private->entry;
	else 
		tmp_entry = *entry;

	/* If we're starting the SPE binary from its original entry point,
	 * setup the arguments to main() */
	if (tmp_entry == spe->base_private->entry &&
			!(spe->base_private->flags &
				(SPE_ISOLATE | SPE_ISOLATE_EMULATE))) {

		addr64 argp64, envp64, tid64, ls64;
		unsigned int regs[128][4];

		/* setup parameters */
		argp64.ull = (uint64_t)(unsigned long)argp;
		envp64.ull = (uint64_t)(unsigned long)envp;
		tid64.ull = (uint64_t)(unsigned long)spe;

		/* make sure the register values are 0 */
		memset(regs, 0, sizeof(regs));

		/* set sensible values for stack_ptr and stack_size */
		regs[1][0] = (unsigned int) LS_SIZE - 16; 	/* stack_ptr */
		regs[2][0] = 0; 							/* stack_size ( 0 = default ) */

		if (runflags & SPE_RUN_USER_REGS) {
			/* When SPE_USER_REGS is set, argp points to an array
			 * of 3x128b registers to be passed directly to the SPE
			 * program.
			 */
			memcpy(regs[3], argp, sizeof(unsigned int) * 12);
		} else {
			regs[3][0] = tid64.ui[0];
			regs[3][1] = tid64.ui[1];

			regs[4][0] = argp64.ui[0];
			regs[4][1] = argp64.ui[1];

			regs[5][0] = envp64.ui[0];
			regs[5][1] = envp64.ui[1];
		}
		
		/* Store the LS base address in R6 */
		ls64.ull = (uint64_t)(unsigned long)spe->base_private->mem_mmap_base;
		regs[6][0] = ls64.ui[0];
		regs[6][1] = ls64.ui[1];

		if (set_regs(spe, regs))
			return -1;
	}

	/*Leave a trail of breadcrumbs for the debugger to follow */
	if (!__spe_current_active_context) {
		__spe_current_active_context = &this_context_info;
		if (!__spe_current_active_context)
			return -1;
		__spe_current_active_context->prev = NULL;
	} else {
		struct spe_context_info *newinfo;
		newinfo = &this_context_info;
		if (!newinfo)
			return -1;
		newinfo->prev = __spe_current_active_context;
		__spe_current_active_context = newinfo;
	}
	/*remember the ls-addr*/
	__spe_current_active_context->spe_id = spe->base_private->fd_spe_dir;

do_run:
	/*Remember the npc value*/
	__spe_current_active_context->npc = tmp_entry;

	/* run SPE context */
	run_rc = spu_run(spe->base_private->fd_spe_dir,
			&tmp_entry, &run_status);

	/*Remember the npc value*/
	__spe_current_active_context->npc = tmp_entry;
	__spe_current_active_context->status = run_status;

	DEBUG_PRINTF("spu_run returned run_rc=0x%08x, entry=0x%04x, "
			"ext_status=0x%04x.\n", run_rc, tmp_entry, run_status);

	/* set up return values and stopinfo according to spu_run exit
	 * conditions. This is overwritten on error.
	 */
	stopinfo->spu_status = run_rc;

	if (spe->base_private->flags & SPE_ISOLATE_EMULATE) {
		/* save the entry point, and pretend that the npc is zero */
		spe->base_private->emulated_entry = tmp_entry;
		*entry = 0;
	} else {
		*entry = tmp_entry;
	}

	/* Return with stopinfo set on syscall error paths */
	if (run_rc == -1) {
		DEBUG_PRINTF("spu_run returned error %d, errno=%d\n",
				run_rc, errno);
		stopinfo->stop_reason = SPE_RUNTIME_FATAL;
		stopinfo->result.spe_runtime_fatal = errno;
		retval = -1;

		/* For isolated contexts, pass EPERM up to the
		 * caller.
		 */
		if (!(spe->base_private->flags & SPE_ISOLATE
				&& errno == EPERM))
			errno = EFAULT;

	} else if (run_rc & SPE_SPU_INVALID_INSTR) {
		DEBUG_PRINTF("SPU has tried to execute an invalid "
				"instruction. %d\n", run_rc);
		stopinfo->stop_reason = SPE_RUNTIME_ERROR;
		stopinfo->result.spe_runtime_error = SPE_SPU_INVALID_INSTR;
		errno = EFAULT;
		retval = -1;

	} else if ((spe->base_private->flags & SPE_EVENTS_ENABLE) && run_status) {
		/* Report asynchronous error if return val are set and
		 * SPU events are enabled.
		 */
		stopinfo->stop_reason = SPE_RUNTIME_EXCEPTION;
		stopinfo->result.spe_runtime_exception = run_status;
		stopinfo->spu_status = -1;
		errno = EIO;
		retval = -1;

	} else if (run_rc & SPE_SPU_STOPPED_BY_STOP) {
		/* Stop & signals are broken down into three groups
		 *  1. SPE library call
		 *  2. SPE user defined stop & signal
		 *  3. SPE program end.
		 *
		 * These groups are signified by the 14-bit stop code:
		 */
		int stopcode = (run_rc >> 16) & 0x3fff;

		/* Check if this is a library callback, and callbacks are
		 * allowed (ie, running without SPE_NO_CALLBACKS)
		 */
		if ((stopcode & 0xff00) == SPE_PROGRAM_LIBRARY_CALL
				&& !(runflags & SPE_NO_CALLBACKS)) {

			int callback_rc, callback_number = stopcode & 0xff;

			/* execute library callback */
			DEBUG_PRINTF("SPE library call: %d\n", callback_number);
			callback_rc = _base_spe_handle_library_callback(spe,
									callback_number, *entry);

			if (callback_rc) {
				/* library callback failed; set errno and
				 * return immediately */
				DEBUG_PRINTF("SPE library call failed: %d\n",
						callback_rc);
				stopinfo->stop_reason = SPE_CALLBACK_ERROR;
				stopinfo->result.spe_callback_error =
					callback_rc;
				errno = EFAULT;
				retval = -1;
			} else {
				/* successful library callback - restart the SPE
				 * program at the next instruction */
				tmp_entry += 4;
				goto do_run;
			}

		} else if ((stopcode & 0xff00) == SPE_PROGRAM_NORMAL_END) {
			/* The SPE program has exited by exit(X) */
			stopinfo->stop_reason = SPE_EXIT;
			stopinfo->result.spe_exit_code = stopcode & 0xff;

			if (spe->base_private->flags & SPE_ISOLATE) {
				/* Issue an isolated exit, and re-run the SPE.
				 * We should see a return value without the
				 * 0x80 bit set. */
				if (!issue_isolated_exit(spe))
					goto do_run;
				retval = -1;
			}

		} else if ((stopcode & 0xfff0) == SPE_PROGRAM_ISOLATED_STOP) {

			/* 0x2206: isolated app has been loaded by loader;
			 * provide a hook for the debugger to catch this,
			 * and restart
			 */
			if (stopcode == SPE_PROGRAM_ISO_LOAD_COMPLETE) {
				__spe_context_update_event();
				goto do_run;
			} else {
				stopinfo->stop_reason = SPE_ISOLATION_ERROR;
				stopinfo->result.spe_isolation_error =
					stopcode & 0xf;
			}

		} else if (spe->base_private->flags & SPE_ISOLATE &&
				!(run_rc & 0x80)) {
			/* We've successfully exited isolated mode */
			retval = 0;

		} else {
			/* User defined stop & signal, including
			 * callbacks when disabled */
			stopinfo->stop_reason = SPE_STOP_AND_SIGNAL;
			stopinfo->result.spe_signal_code = stopcode;
			retval = stopcode;
		}

	} else if (run_rc & SPE_SPU_HALT) {
		DEBUG_PRINTF("SPU was stopped by halt. %d\n", run_rc);
		stopinfo->stop_reason = SPE_RUNTIME_ERROR;
		stopinfo->result.spe_runtime_error = SPE_SPU_HALT;
		errno = EFAULT;
		retval = -1;

	} else if (run_rc & SPE_SPU_WAITING_ON_CHANNEL) {
		DEBUG_PRINTF("SPU is waiting on channel. %d\n", run_rc);
		stopinfo->stop_reason = SPE_RUNTIME_EXCEPTION;
		stopinfo->result.spe_runtime_exception = run_status;
		stopinfo->spu_status = -1;
		errno = EIO;
		retval = -1;

	} else if (run_rc & SPE_SPU_INVALID_CHANNEL) {
		DEBUG_PRINTF("SPU has tried to access an invalid "
				"channel. %d\n", run_rc);
		stopinfo->stop_reason = SPE_RUNTIME_ERROR;
		stopinfo->result.spe_runtime_error = SPE_SPU_INVALID_CHANNEL;
		errno = EFAULT;
		retval = -1;

	} else {
		DEBUG_PRINTF("spu_run returned invalid data: 0x%04x\n", run_rc);
		stopinfo->stop_reason = SPE_RUNTIME_FATAL;
		stopinfo->result.spe_runtime_fatal = -1;
		stopinfo->spu_status = -1;
		errno = EFAULT;
		retval = -1;

	}

	freespeinfo();
	return retval;
}
