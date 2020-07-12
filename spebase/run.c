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
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include <sys/types.h>

#include <sys/spu.h>

#include "elf_loader.h"
#include "run.h"
#include "lib_builtin.h"
#include "spebase.h"

int set_regs(struct spe_context *spe, char *regs)
{
	int fd_regs, rc;

	fd_regs=openat(spe->base_private->fd_spe_dir, "regs", O_RDWR);
	if (fd_regs < 0) {
		DEBUG_PRINTF ("Could not open SPE regs file.\n");
		errno=EFAULT;
		return -1;
	}

	rc = write(fd_regs,regs,2048);

	close(fd_regs);

	if ( rc < 0 ) {
		return -1;
	}
	
	return 0;
}

static void issue_isolated_exit(struct spe_context *spe)
{
	struct spe_spu_control_area *cntl_area =
		spe->base_private->cntl_mmap_base;

	if (!cntl_area) {
		DEBUG_PRINTF ("%s: could not access SPE control area\n",
				__FUNCTION__);
		return;
	}

	/* 0x2 is an isolated exit request */
	cntl_area->SPU_RunCntl = 0x2;
}

int _base_spe_context_run(spe_context_ptr_t spe, unsigned int *entry, 
					unsigned int runflags, void *argp, void *envp, spe_stop_info_t *stopinfo)
{
	unsigned int regs[128][4];
	int 		ret;
	addr64 		argp64, envp64, tid64;

	unsigned int spu_extended_status=0;
	int retval = 0;
	
	
	int run_again = 1;
	int stopcode = 0;
	spe_stop_info_t stopinfo_buf;

	/* SETUP parameters */

	argp64.ull = (unsigned long long) (unsigned long) argp;
	envp64.ull = (unsigned long long) (unsigned long) envp;
	tid64.ull = (unsigned long long) (unsigned long) spe;

	if (!stopinfo) {
		stopinfo = &stopinfo_buf;
	}
	
	if (*entry == SPE_DEFAULT_ENTRY){
		*entry = spe->base_private->entry;
	}

	if (*entry==spe->base_private->entry){
		if (!(spe->base_private->flags & SPE_ISOLATE)){
			/* make sure the register values are 0 */
			memset(regs, 0, sizeof(regs));
			if (runflags & SPE_RUN_USER_REGS) {
				/* When flags & SPE_USER_REGS is set, argp points
				 * to an array of 3x128b registers to be passed
			 	 * directly to the SPE program.
				 */
				memcpy(regs[3], argp, sizeof(unsigned int) * 12);
			} else {
				regs[3][0] = tid64.ui[0];
				regs[3][1] = tid64.ui[1];
				regs[3][2] = 0;
				regs[3][3] = 0;
				
				regs[4][0] = argp64.ui[0];
				regs[4][1] = argp64.ui[1];
				regs[4][2] = 0;
				regs[4][3] = 0;
				
			 	regs[5][0] = envp64.ui[0];
				regs[5][1] = envp64.ui[1];
				regs[5][2] = 0;
				regs[5][3] = 0;
			}
			if (set_regs(spe, (char*) regs)) {
				return -1;
			}
		}
	}

	while ( run_again ) {
		run_again = 0;
	
		// run SPE context
		if ( spe->base_private->flags & SPE_EVENTS_ENABLE ) {
			// spu_run will return error event code in error_info
			ret = spu_run(spe->base_private->fd_spe_dir, entry, &spu_extended_status);
		} else {
		  	// spu_run will generate a signal in case of an error event
		  	ret = spu_run(spe->base_private->fd_spe_dir, entry, NULL);
		}
		
		DEBUG_PRINTF ("spu_run returned ret=0x%08x, entry=0x%04x, ext_status=0x%04x.\n",
						 ret, *entry, spu_extended_status);
		
		// set up return values and stopinfo according to spu_run exit conditions
		// Remember this - overwrite if invalid.
		stopinfo->spu_status = ret;
		
		if (ret == -1) { /*Test for common error*/
			// spu_run returned and error (-1) and has set errno;
			DEBUG_PRINTF ("spu_run returned error %d, errno=%d\n", ret, errno);
			// return immediately with stopinfo set correspondingly
			if ( errno != 0 && (spe->base_private->flags & SPE_EVENTS_ENABLE) ) {
				// if errno == EIO and SPU events enabled ==> asynchronous error
				stopinfo->stop_reason = SPE_RUNTIME_EXCEPTION;
				stopinfo->result.spe_runtime_exception = spu_extended_status;
				stopinfo->spu_status = -1;
				errno = EIO;
			} else {
				// else other fatal runtime error
				stopinfo->stop_reason = SPE_RUNTIME_FATAL;
				stopinfo->result.spe_runtime_fatal = errno;
				stopinfo->spu_status = -1;
				errno = EFAULT;
			}
			return -1;
		} 

		if ( (spe->base_private->flags & SPE_EVENTS_ENABLE) && ret == 0 && (spu_extended_status & 0xFFF)) {
			// if upper 16bit of return val set and SPU events enabled ==> asynchronous error
			stopinfo->stop_reason = SPE_RUNTIME_EXCEPTION;
			stopinfo->result.spe_runtime_exception = spu_extended_status;
			stopinfo->spu_status = -1;
			errno = EIO;
			return -1;
		}

		/*   spu_run defines the follwoing return values:
		 *   0x02   SPU was stopped by stop-and-signal.
		 *   0x04   SPU was stopped by halt.
		 *   0x08   SPU is waiting for a channel.
		 *   0x10   SPU is in single-step mode.
		 *   0x20   SPU has tried to execute an invalid instruction.
		 *   0x40   SPU has tried to access an invalid channel. 
		 */
		else if (ret & SPE_SPU_STOPPED_BY_STOP) { /*Test for stop&signal*/
			/* Stop&Signals are broken down into tree groups
			 * 1. SPE library calls 
			 * 2. SPE user defined Stop&Signals
			 * 3. SPE program end
			 */
			
			// SPU stopped by stop&signal; get 14-bit stop code
			stopcode = ( ret >> 16 ) & 0x3fff;
			// check if this is a library callback (stopcode has 0x2100 bits set)
			// and callbacks are allowed (SPE_NO_CALLBACKS - don't run any library call functions)
			if ((stopcode & 0xff00) == SPE_PROGRAM_LIBRARY_CALL
					&& !(runflags & SPE_NO_CALLBACKS)) {
				int rc, callnum;

				// execute library callback
				callnum = stopcode & 0xff;
				DEBUG_PRINTF ("SPE library call: %d\n",callnum);
				rc = handle_library_callback(spe, callnum, entry);
				if (rc) {
					/* library callback failed;
					 * set errno and return immediately */
					DEBUG_PRINTF("SPE library call "
							"failed: %d\n", rc);
					stopinfo->stop_reason =
						SPE_CALLBACK_ERROR;
					stopinfo->result.spe_callback_error =
						rc;
					stopinfo->spu_status = ret;
					errno = EFAULT;
					return -1;
				} else {
					/* successful library callback -
					 * restart SPE program */
					run_again = 1;
					*entry += 4;
				}

			} else if ((stopcode & 0xff00)
					== SPE_PROGRAM_NORMAL_END) {
				// this SPE program has ended.
				// SPE program stopped by exit(X)
				stopinfo->stop_reason = SPE_EXIT;
				stopinfo->result.spe_exit_code =
					stopcode & 0xff;
				stopinfo->spu_status = ret;
				errno = 0;

				if (spe->base_private->flags & SPE_ISOLATE) {
					issue_isolated_exit(spe);
					run_again = 1;
				} else {
					return 0;
				}

			} else if (spe->base_private->flags & SPE_ISOLATE &&
					!(ret & 0x80)) {
				/* we've successfully exited isolated mode */
				return errno ? -1 : 0;

			} else {
				/* user defined stop & signal, including
				 * callbacks when disabled */
				stopinfo->stop_reason = SPE_STOP_AND_SIGNAL;
				stopinfo->result.spe_signal_code = stopcode;
				retval = stopcode;
				errno = 0;
			}

		} else if (ret & SPE_SPU_HALT) { /*Test for halt*/
			//   0x04   SPU was stopped by halt.
			DEBUG_PRINTF("0x04   SPU was stopped by halt.%d\n", ret);
			stopinfo->stop_reason = SPE_RUNTIME_ERROR;
			stopinfo->result.spe_runtime_error = SPE_SPU_HALT;
			errno = EFAULT;
			return -1;
		} else if (ret & SPE_SPU_WAITING_ON_CHANNEL) { /*Test for wait on channel*/
			//   0x08   SPU is waiting for a channel.
			DEBUG_PRINTF("0x04   SPU is waiting on channel. %d\n", ret);
			stopinfo->stop_reason = SPE_RUNTIME_EXCEPTION;
			stopinfo->result.spe_runtime_exception = spu_extended_status;
			stopinfo->spu_status = -1;
			errno = EIO;
			return -1;
		} else if (ret & SPE_SPU_INVALID_INSTR) { /*Test for invalid instr.*/
		    //   0x20   SPU has tried to execute an invalid instruction.
		    DEBUG_PRINTF("0x20   SPU has tried to execute an invalid instruction.%d\n", ret);
		    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
		    stopinfo->result.spe_runtime_error = SPE_SPU_INVALID_INSTR;
		    errno = EFAULT;
			return -1;
		} else if (ret & SPE_SPU_INVALID_CHANNEL) { /*Test for invalid channel*/
	    //   0x40   SPU has tried to access an invalid channel.
			DEBUG_PRINTF("0x40   SPU has tried to access an invalid channel.%d\n", ret);
			stopinfo->stop_reason = SPE_RUNTIME_ERROR;
			stopinfo->result.spe_runtime_error = SPE_SPU_INVALID_CHANNEL;
			errno = EFAULT;
			return -1;
		} else { /*ERROR - SPE Stopped without a specific reason*/
			DEBUG_PRINTF ("spu_run returned invalid data ret=0x%04x\n", ret);
			stopinfo->stop_reason = SPE_RUNTIME_FATAL;
			stopinfo->result.spe_runtime_fatal = -1;
			stopinfo->spu_status = -1;
			errno = EFAULT;
			return -1;
		}

	} /* while (run_again) */

  return retval;
}
/*
  // OK - now we have handled spu_run errors and library callbacks
  // READY to return from spe_context_run
  // set up stopinfo and return values


  // set up return values and stopinfo according to spu_run exit conditions
  // (special cases have been handled above inside loop)
  //   spu_run defines the follwoing return values:
  //   0x02   SPU was stopped by stop-and-signal.
  //   0x04   SPU was stopped by halt.
  //   0x08   SPU is waiting for a channel.
  //   0x10   SPU is in single-step mode.
  //   0x20   SPU has tried to execute an invalid instruction.
  //   0x40   SPU has tried to access an invalid channel.

 
  switch ( ret & 0xff ) {
  case 0x02:
    //   0x02   SPU was stopped by stop-and-signal.
    DEBUG_PRINTF("0x04   SPU was stopped by stop&signal.%d\n", ret);
    stopcode = ( ret >> 16 ) & 0x3fff;
    if ( stopcode & SPE_PROGRAM_NORMAL_END ) {
      stopinfo->spe_exit_code = stopcode & 0xff;
      // SPE program stopped by exit(X)
      if ( stopinfo->spe_exit_code == 0 ) {
        // SPE exit(0)
        stopinfo->stop_reason = SPE_EXIT_NORMAL;
        retval = 0;
        errno = 0;
      } else {
        // SPE non-zero exit value
        stopinfo->stop_reason = SPE_EXIT_WITH_CODE;
        retval = -1;
        errno = ECANCELED;
      }
    } else {
      // SPE program stopped by (user-defined) stop&signal
      stopinfo->stop_reason = SPE_STOP_AND_SIGNAL;
      stopinfo->spe_signal = stopcode;
      retval = stopcode;
      errno = 0;
    };
    break;
  case 0x04:
    //   0x04   SPU was stopped by halt.
    DEBUG_PRINTF("0x04   SPU was stopped by halt.%d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = 0x04;
    retval = -1;
    errno = EFAULT;
    break;
  case 0x08:
    //   0x08   SPU is waiting for a channel.
    DEBUG_PRINTF("0x08   SPU is waiting for a channel.%d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = 0x08;
    retval = -1;
    errno = EFAULT;
    break;
  case 0x10:
    //   0x10   SPU is in single-step mode.
    DEBUG_PRINTF("0x10   SPU is in single-step mode.%d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = 0x10;
    retval = -1;
    errno = EFAULT;
    break;
  case 0x20:
    //   0x20   SPU has tried to execute an invalid instruction.
    DEBUG_PRINTF("0x20   SPU has tried to execute an invalid instruction.%d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = 0x20;
    retval = -1;
    errno = EFAULT;
    break;
  case 0x40:
    //   0x40   SPU has tried to access an invalid channel.
    DEBUG_PRINTF("0x40   SPU has tried to access an invalid channel.%d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = 0x40;
    retval = -1;
    errno = EFAULT;
    break;
  default:
    // unspecified return value from kernel interface
    DEBUG_PRINTF("spu_run returned unknown return value %d\n", ret);
    stopinfo->stop_reason = SPE_RUNTIME_ERROR;
    stopinfo->spe_runtime_error = (ret & 0xff);
    retval = -1;
    errno = EFAULT;
  }

  // done - get outta here...
  return retval;
}
*/

