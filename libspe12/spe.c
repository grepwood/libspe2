/*
 * libspe - A wrapper library to adapt the JSRE SPU usage model to SPUFS
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

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

#include <linux/unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>


#include "libspe.h"
#include "create.h"
#include "spe.h"


/*
 * Helpers
 *
 * */


int __env_spu_debug_start = 0;
int __env_spu_info = 0;
int check_env = 1;

extern int default_priority;
extern int default_policy;
extern int default_eventmask;


void
env_check(void)
{
	if (getenv("SPU_DEBUG_START"))
		__env_spu_debug_start = strtol ((const char *)getenv("SPU_DEBUG_START"),(char **)NULL, 0);
	if (getenv("SPU_INFO"))
		__env_spu_info = strtol ((const char *)getenv("SPU_INFO"),(char **)NULL, 0);
	check_env = 0;
}




/**
 * Low-level API
 * 
 */

void * spe_setup (spe_gid_t gid, spe_program_handle_t *handle, void *argp, void *envp, int flags)
{
	struct thread_store *thread_store;
	int rc, sigfd;
	
	if(!gid) {
		gid=spe_gid_setup(default_policy,default_priority,default_eventmask);
	} else {
		struct group_store *grp = gid;
		if (grp->numListSize == MAX_THREADS_PER_GROUP) {
		  	DEBUG_PRINTF ("Thread group has reached maximum number of members.\n");
		  	errno = ENOMEM;
		  	return NULL;
		}
	}

	/*Allocate thread structure */
	thread_store = calloc (1, sizeof *thread_store);
	if (!thread_store) {
		DEBUG_PRINTF ("Could not allocate thread store\n");
		errno = ENOMEM;
		return NULL;
	}

	flags = SPE_EVENTS_ENABLE | flags;

	thread_store->group_id = gid;
	thread_store->flags = flags;
	DEBUG_PRINTF ("thread_store->flags %08x\n", thread_store->flags);
	
	thread_store->argp = argp;
	thread_store->envp = envp;

	thread_store->spectx = _base_spe_context_create(flags,NULL,NULL);

	if (!thread_store->spectx) {
		DEBUG_PRINTF ("Could not create SPE\n");
		return NULL;
	}
	
	rc = _base_spe_program_load(thread_store->spectx, handle);
	
	if (rc) {
		DEBUG_PRINTF ("Could not load SPE\n");
		return NULL;		
	}

	rc = pipe(thread_store->spectx->base_private->ev_pipe);
	if (rc)
		thread_store->spectx->base_private->ev_pipe[0] = thread_store->spectx->base_private->ev_pipe[1] = -1;

	rc = add_thread_to_group(gid, thread_store);

	/* Register SPE image start address as "object-id" for oprofile.  */
	//sprintf (signame, "%s/object-id", pathname);
	sigfd = openat (__base_spe_spe_dir_get(thread_store->spectx),"object-id", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (sigfd >= 0) {
		char buf[100];
		sprintf (buf, "%p", handle->elf_image);
		write (sigfd, buf, strlen (buf) + 1);
		close (sigfd);
	}

	thread_store->stop = 0;
	return thread_store;
}

void spe_cleanup (void *ptr)
{
	struct thread_store *thread_store = ptr;
	
	_base_spe_context_destroy(thread_store->spectx);

	remove_thread_from_group(thread_store->group_id, thread_store);

	free (thread_store);
}

int do_spe_run (void *ptr)
{
	struct thread_store *thread_store = ptr;

	unsigned int npc = SPE_DEFAULT_ENTRY;
	spe_stop_info_t stop_info;

	int ret;

	DEBUG_PRINTF ("do_spe_run\n");
	thread_store->thread_status = 1; 
	
	/* Note: in order to be able to kill a running SPE thread the 
	 *       spe thread must be cancelable.
	 */
	
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	
	do {
		thread_store->thread_status = 2; 
		
		DEBUG_PRINTF ("calling _base_spe_context_run\n");
		ret = _base_spe_context_run(thread_store->spectx, &npc,0, thread_store->argp, thread_store->envp, &stop_info);

		DEBUG_PRINTF ("_base_spe_context_run() result: npc=0x%08x ret=%08x\n", npc, ret);
		
		thread_store->thread_status = 10; //SPE _not_ running.
		
		/* If a signal was sent to STOP execution we will pause and wait 
		 * for a continue signal before going on.
		 */

		if ( thread_store->stop ) {
			pthread_mutex_lock(&thread_store->event_lock);
			pthread_cond_init(&thread_store->event_deliver, NULL);
			
			thread_store->thread_status = 20; //SPE _pausing_.
			/* Now wait for the continue signal */
			
			pthread_cond_wait(&thread_store->event_deliver, &thread_store->event_lock);
			pthread_mutex_unlock(&thread_store->event_lock);
			thread_store->stop = 0;
			thread_store->thread_status = 10; //SPE _not_ running.
		}
                
		if (ret < 0) {
			pthread_mutex_lock(&thread_store->event_lock);
			pthread_cond_init(&thread_store->event_deliver, NULL);
			switch(errno){
				case EIO:
					DEBUG_PRINTF ("received EIO. stop_info.result.spe_runtime_exception %d \n", 
						      stop_info.result.spe_runtime_exception );
					thread_store->event = stop_info.result.spe_runtime_exception;
					DEBUG_PRINTF ("thread_store->event %X\n", &thread_store->event);
					thread_store->ev_data = stop_info.result.spe_runtime_exception;
					write(thread_store->spectx->base_private->ev_pipe[1], &thread_store->event, 4);
				break;
				case EFAULT:
					DEBUG_PRINTF ("received EFAULT. stop_info.result.spe_runtime_exception %d \n", 
						      stop_info.result.spe_runtime_exception );
					switch(stop_info.result.spe_runtime_exception){
					case 4:
						thread_store->event = SPE_EVENT_SPE_TRAPPED;
						break;
					case 32:
						thread_store->event = SPE_EVENT_SPE_ERROR;
						break;
					default:
						thread_store->event = stop_info.result.spe_runtime_exception;
					}
					DEBUG_PRINTF ("thread_store->event %X\n", &thread_store->event);
					thread_store->ev_data = stop_info.result.spe_runtime_exception;
					write(thread_store->spectx->base_private->ev_pipe[1], &thread_store->event, 4);
				break;

				default:
					DEBUG_PRINTF ("received %d.\n", errno);
				}
			pthread_mutex_unlock(&thread_store->event_lock);
			thread_store->stop = 1;
			thread_store->thread_status = 99; 
			return stop_info.spu_status;
		}// else if (ret > 0) {
		if (ret > 0) {
			pthread_mutex_lock(&thread_store->event_lock);
			pthread_cond_init(&thread_store->event_deliver, NULL);
			
			if (((struct group_store*)thread_store->group_id)->use_events) {
				int callnum = ret;
				//
				// Event - driven 21xx hanlers
				//
				thread_store->event = SPE_EVENT_STOP;
				thread_store->ev_data = callnum;
				//
				//Todo Error handling
				//
				write(thread_store->spectx->base_private->ev_pipe[1], &thread_store->event, 4);
				
				//Show that we're waiting
				thread_store->event_pending = 1;
				
				// Wait for a signal.
				pthread_cond_wait(&thread_store->event_deliver, &thread_store->event_lock);

				thread_store->event_pending = 0;
				
				pthread_mutex_unlock(&thread_store->event_lock);
			} else {
				pthread_mutex_unlock(&thread_store->event_lock);
				
				DEBUG_PRINTF ("SPE user events not enabled.\n");
				thread_store->thread_status = 99; 
			        return -ENOSYS;
			}
		}
	} while (ret != 0 && !thread_store->killed );

	DEBUG_PRINTF ("SPE thread result: %08x:%04x\n", npc, ret);
	
	/* Save status & npc */	
	thread_store->npc = npc;
	thread_store->ret_status = stop_info.spu_status;
	
	//
	//Event code
	//
	if (((struct group_store*)thread_store->group_id)->use_events) {
		thread_store->event = SPE_EVENT_THREAD_EXIT;
		thread_store->ev_data = stop_info.stop_reason;
		//
		//Todo Error handling
		//
		write(thread_store->spectx->base_private->ev_pipe[1], &thread_store->event, 4);
	}

	thread_store->thread_status = 99; 
	return stop_info.spu_status;
}


struct image_handle {
	spe_program_handle_t speh;
	unsigned int map_size;
};

/**
 * spe_open_image maps an SPE ELF executable indicated by filename into system 
 * memory and returns the mapped address appropriate for use by the spe_create_thread 
 * API. It is often more convenient/appropriate to use the loading methodologies 
 * where SPE ELF objects are converted to PPE static or shared libraries with 
 * symbols which point to the SPE ELF objects after these special libraries are 
 * loaded. These libraries are then linked with the associated PPE code to provide 
 * a direct symbol reference to the SPE ELF object. The symbols in this scheme 
 * are equivalent to the address returned from the spe_open_image function.
 * SPE ELF objects loaded using this function are not shared with other processes, 
 * but SPE ELF objects loaded using the other scheme, mentioned above, can be 
 * shared if so desired.
 * 
 * @param filename Specifies the filename of an SPE ELF executable to be loaded 
 * and mapped into system memory.
 * 
 * @return On success, spe_open_image returns the address at which the specified 
 * SPE ELF object has been mapped. On failure, NULL is returned and errno is set 
 * appropriately.
 * Possible errors include:
 * @retval EACCES The calling process does not have permission to access the 
 * specified file.
 * @retval EFAULT The filename parameter points to an address that was not 
 * contained in the calling process’s address space.
 * 
 * A number of other errno values could be returned by the open(2), fstat(2), 
 * mmap(2), munmap(2), or close(2) system calls which may be utilized by the 
 * spe_open_image or spe_close_image functions.
 * @sa spe_create_thread
 */
spe_program_handle_t *spe_open_image(const char *filename)
{
	return _base_spe_image_open(filename);
}

/**
 * spe_close_image unmaps an SPE ELF object that was previously mapped using 
 * spe_open_image.
 * @param handle handle to open file
 * 
 * @return On success, spe_close_image returns 0. On failure, -1 is returned and errno is 
 * set appropriately.
 * @retval EINVAL From spe_close_image, this indicates that the file, specified by 
 * filename, was not previously mapped by a call to spe_open_image.
 */
int spe_close_image(spe_program_handle_t *handle)
{
	return _base_spe_image_close(handle);
}

void register_handler(void * handler, unsigned int callnum ) 
{
	_base_spe_callback_handler_register(handler, callnum, SPE_CALLBACK_NEW);
}

/**
 * Library API
 * 
 */


void * spe_thread (void *ptr)
{
	/* If the SPU_INFO (or SPU_DEBUG_START) environment variable is set,
	   output a message to stderr telling the user how to attach a debugger
	   to the new SPE thread.  */
	if (__env_spu_debug_start || __env_spu_info) {
		int tid = syscall (__NR_gettid);
		fprintf (stderr, "Starting SPE thread %p, to attach debugger use: spu-gdb -p %d\n",
			 ptr, tid);

		/* In the case of SPU_DEBUG_START, actually wait until the user *has*
		   attached a debugger to this thread.  This is done here by doing an
		   sigwait on the empty set, which will return with EINTR after the
		   debugger has attached.  */
		if (__env_spu_debug_start) {
			sigset_t set;
			sigemptyset (&set);
			/* Use syscall to avoid glibc looping on EINTR.  */
			syscall (__NR_rt_sigtimedwait, &set, (void *) 0,
				 (void *) 0, _NSIG / 8);
		}
	}
	return (void *) (unsigned long) do_spe_run (ptr);
}

/**
 * The spe_get_ls function returns the address of the local storage 
 * for the SPE thread indicated by speid.
 * 
 * @param speid The identifier of a specific SPE thread.
 * @return On success, a non-NULL pointer is returned. On failure, NULL is returned 
 * and errno is set appropriately. Possible errors include:
 * @retval ENOSYS Access to the local store of an SPE thread is not supported by 
 * the operating system.
 * @retval ESRCH The specified SPE thread could not be found.
 * @sa spe_create_group, spe_get_ps_area
 */
void *spe_get_ls (speid_t speid)
{
	struct thread_store *thread_store = speid;
	
	if (!srch_thread(speid))
		return NULL;
	
	return _base_spe_ls_area_get(thread_store->spectx);
}

/**
 * The spe_get_context call returns the SPE user context for an SPE thread.
 * @param speid Specifies the SPE thread
 * @param uc Points to the SPE user context structure.
 * 
 * @return On failure, -1 is returned and errno is set appropriately.
 * 
 * @sa spe_kill, spe_create_thread, spe_wait, getcontext, setcontect
 */
int spe_get_context(speid_t speid, struct spe_ucontext *uc)
{
	printf("spe_get_context: not implemented in this release.\n");

	errno=ENOSYS;
	return -1;
}

/**
 * The spe_set_context call sets the SPE user context for an SPE thread.
 * @param speid Specifies the SPE thread
 * @param uc Points to the SPE user context structure.
 * 
 * @return On failure, -1 is returned and errno is set appropriately.
 * 
 * @sa spe_kill, spe_create_thread, spe_wait, getcontext, setcontect
 */
int spe_set_context(speid_t speid, struct spe_ucontext *uc)
{
	printf("spe_set_context: not implemented in this release.\n");

	errno=ENOSYS;
	return -1;
}

int __spe_get_context_fd(speid_t speid)
{
	struct thread_store *thread_store = speid;
	
	if (!srch_thread(speid))
		return -1;

	return __base_spe_spe_dir_get(thread_store->spectx);
}
		

/*
 * mfc.h direct call-ins
 *
 * */

/**
 * The spe_write_in_mbox function places the 32-bit message specified by data into 
 * the SPU inbound mailbox for the SPE thread specified by the speid parameter.
 * If the mailbox is full, then spe_write_in_mbox can overwrite the last entry in 
 * the mailbox. The spe_stat_in_mbox function can be called to ensure that space is 
 * available prior to writing to the inbound mailbox.
 * 
 * @param speid Specifies the SPE thread whose outbound mailbox is to be read.
 * @param data The 32-bit message to be written into the SPE’s inbound mailbox.
 * @return On success, spe_write_in_mbox returns 0. On failure, -1 is returned.
 * @sa spe_read_out_mbox, spe_stat_in_mbox. Spe_stat_out_mbox, 
 * spe_stat_out_intr_mbox, write (2)
 */
int spe_write_in_mbox (speid_t speid ,unsigned int data)
{
	struct thread_store *thread_store = speid;
	int rc;
	
	if (!srch_thread(speid))
		return -1;
	
	if (thread_store->thread_status == 99)
		return -1;

	rc = _base_spe_in_mbox_write(thread_store->spectx, &data, 1, SPE_MBOX_ALL_BLOCKING);

	if (rc == 1)
		rc = 0;

	return rc;
}

/**
 * The spe_stat_in_mbox function fetches the status of the SPU inbound mailbox for 
 * the SPE thread specified by the speid parameter. A 0 value is return if the 
 * mailbox is full. A non-zero value specifies the number of available (32-bit) 
 * mailbox entries.
 * 
 * @param speid Specifies the SPE thread whose mailbox status is to be read.
 * @return On success, returns the current status of the mailbox, respectively. 
 * On failure, -1 is returned.
 * @sa spe_read_out_mbox, spe_write_in_mbox, read (2)
 */
int spe_stat_in_mbox (speid_t speid)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
		return -1;
	
	return _base_spe_in_mbox_status(thread_store->spectx);
}

/**
 * The spe_read_out_mbox function returns the contents of the SPU outbound mailbox 
 * for the SPE thread specified by the speid parameter. This read is non-blocking 
 * and returns -1 if no mailbox data is available.
 * The spe_stat_out_mbox function can be called to ensure that data is available 
 * prior to reading the outbound mailbox.
 * @param speid Specifies the SPE thread whose outbound mailbox is to be read.
 * @return On success, spe_read_out_mbox returns the next 32-bit mailbox message. 
 * On failure, -1 is returned.
 * @sa spe_stat_in_mbox, spe_stat_out_mbox, spe_stat_out_intr_mbox,
 * spe_write_in_mbox, read (2)
 */
unsigned int spe_read_out_mbox (speid_t speid)
{
    struct thread_store *thread_store = speid;
    int rc;
	unsigned int ret;

	if (!srch_thread(speid))
		return -1;

	rc = _base_spe_out_mbox_read(thread_store->spectx, &ret, 1);
    
	if (rc != 1)
		ret = -1;

	return ret;
}


/**
 * The spe_stat_out_mbox function fetches the status of the SPU outbound mailbox 
 * for the SPE thread specified by the speid parameter. A 0 value is return if 
 * the mailbox is empty. A non-zero value specifies the number of 32-bit unread 
 * mailbox entries.
 * 
 * @param speid Specifies the SPE thread whose mailbox status is to be read.
 * @return On success, returns the current status of the mailbox, respectively. 
 * On failure, -1 is returned.
 * @sa spe_read_out_mbox, spe_write_in_mbox, read (2)
 */
int spe_stat_out_mbox (speid_t speid)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
		return -1;

	return _base_spe_out_mbox_status(thread_store->spectx);
}


/**
 * The spe_stat_out_intr_mbox function fetches the status of the SPU outbound 
 * interrupt mailbox for the SPE thread specified by the speid parameter. A 0 
 * value is return if the mailbox is empty. A non-zero value specifies the number 
 * of 32-bit unread mailbox entries.
 * 
 * @param speid Specifies the SPE thread whose mailbox status is to be read.
 * @return On success, returns the current status of the mailbox, respectively. 
 * On failure, -1 is returned.
 * @sa spe_read_out_mbox, spe_write_in_mbox, read (2)
 */
int spe_stat_out_intr_mbox (speid_t speid)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
		return -1;

	return _base_spe_out_intr_mbox_status(thread_store->spectx);
}


/**
 * The spe_write_signal function writes data to the signal notification register 
 * specified by signal_reg for the SPE thread specified by the speid parameter.
 * 
 * @param speid Specifies the SPE thread whose signal register is to be written to.
 * @param signal_reg Specified the signal notification register to be written. 
 * Valid signal notification registers are:\n
 * SPE_SIG_NOTIFY_REG_1 SPE signal notification register 1\n
 * SPE_SIG_NOTIFY_REG_2 SPE signal notification register 2\n
 * @param data The 32-bit data to be written to the specified signal notification 
 * register.
 * @return On success, spe_write_signal returns 0. On failure, -1 is returned.
 * @sa spe_get_ps_area, spe_write_in_mbox
 */ 
int spe_write_signal (speid_t speid, unsigned int signal_reg, unsigned int data)
{
	struct thread_store *thread_store = speid;
	
	if (!srch_thread(speid))
		return -1;
	
	if (thread_store->thread_status == 99)
		return -1;

	return _base_spe_signal_write(thread_store->spectx, signal_reg, data);
}

/*
 * 
 */
 
