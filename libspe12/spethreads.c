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
#include <dirent.h>
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
#include "elf_loader.h"

/*
 * Helpers
 *
 * */

static struct group_list grp_list = {NULL,PTHREAD_MUTEX_INITIALIZER,0};

extern int check_env;

/* Group defaults
 *  */

int default_priority = 0;
int default_policy   = SCHED_OTHER;
int default_eventmask = 0;

//extern void * spe_thread (void *);

int 
add_group_to_groups(struct group_store *gid)
{
	struct grpsElem *listElem;
	listElem = malloc(sizeof *listElem);

	if (!listElem)
	{
		errno=ENOMEM;
		return -errno;
	}
	
	pthread_mutex_lock(&grp_list.mutex);

	listElem->group=gid;
	listElem->next=grp_list.list_start;
	
	grp_list.list_start=listElem;
	grp_list.numListSize++;
		
	pthread_mutex_unlock(&grp_list.mutex);
	
	return 0;
	
}
int 
add_thread_to_group(struct group_store *gid, struct thread_store *thread)
{
	struct grpListElem *listElem;
	listElem = malloc(sizeof *listElem);

	if (!listElem)
	{
		errno=ENOMEM;
		return -errno;
	}

	pthread_mutex_lock(&grp_list.mutex);
	
	listElem->thread=thread;
	listElem->next=gid->grp_members;

	gid->grp_members=listElem;
	gid->numListSize++;
		
	pthread_mutex_unlock(&grp_list.mutex);
	
	return 0;
}

int 
remove_group_from_groups(struct group_store *gid)
{
	pthread_mutex_lock(&grp_list.mutex);
	
	struct grpsElem *lgp = NULL;
	
	for (struct grpsElem *gp=grp_list.list_start ; gp !=NULL; gp=gp->next)
	{
		if (gp->group == gid)
		{
			if (gp->group->numListSize == 0)
			{
				// Delete the group
				
				if (lgp == NULL)
				{
					grp_list.list_start = gp->next;
				}
				else
				{
					lgp->next = gp->next; 
				}

				free(gp);
			}
			
			pthread_mutex_unlock(&grp_list.mutex);		
			return 0;
		}
		lgp = gp;
	}

	pthread_mutex_unlock(&grp_list.mutex);
	
	return 1;
}

int 
remove_thread_from_group(struct group_store *gid, struct thread_store *thread)
{
	pthread_mutex_lock(&grp_list.mutex);
	
	if (gid->numListSize == 1)
	{	
		struct grpListElem *p=gid->grp_members;
		gid->numListSize--;
		free(p);
		gid->grp_members = NULL;
		pthread_mutex_unlock(&grp_list.mutex);
		
		if (gid->deleteMe)
		{
			remove_group_from_groups(gid);
		}
		return 0;
	}

	for (struct grpListElem *p=gid->grp_members; p->next!=NULL; p=p->next)
	{
		struct grpListElem *np=p->next;
		if (np->thread==thread)
		{
			p->next=np->next;
			gid->numListSize--;
			free(np);
			
			pthread_mutex_unlock(&grp_list.mutex);

			return 0;
		}
	}

	pthread_mutex_unlock(&grp_list.mutex);

	errno = ESRCH;
	return -ESRCH;
}

struct thread_store *
srch_thread(struct thread_store *thread)
{
	pthread_mutex_lock(&grp_list.mutex);

	for (struct grpsElem *gp=grp_list.list_start ; gp !=NULL; gp=gp->next)
	{
		//printf("1\n");
		struct group_store *gid = gp->group;
			
		for (struct grpListElem *p=gid->grp_members; p !=NULL; p=p->next)
		{
			//printf("2\n");
			if (p->thread==thread)
			{
				pthread_mutex_unlock(&grp_list.mutex);		
				return p->thread;
			}
		}
	}
	
	pthread_mutex_unlock(&grp_list.mutex);
	errno = ESRCH;
	return NULL;
}

struct group_store *
srch_group(struct group_store *group)
{
	pthread_mutex_lock(&grp_list.mutex);

	for (struct grpsElem *gp=grp_list.list_start ; gp !=NULL; gp=gp->next)
	{
		if (gp->group == group)
		{
			pthread_mutex_unlock(&grp_list.mutex);		
			return gp->group;
		}
	}
	
	pthread_mutex_unlock(&grp_list.mutex);
	errno = ESRCH;
	return NULL;
}


void *
spe_gid_setup(int policy, int priority, int use_events)
{
	struct group_store *group_store;

        group_store = calloc (1, sizeof *group_store);
	if (!group_store)
	{
		DEBUG_PRINTF ("Could not allocate group store\n");
		errno = ENOMEM;
	        return NULL;
	}
	
	group_store->policy = policy;
	group_store->priority = priority;
	group_store->use_events = use_events;
	group_store->grp_members = NULL;
	group_store->numListSize = 0;
	group_store->deleteMe = 0;

	add_group_to_groups(group_store);
	
	return group_store;
}

/**
 * The spe_create_group function allocates a new SPE thread group. SPE thread groups 
 * define the scheduling policies and priorities for a set of SPE threads. Each SPE 
 * thread belongs to exactly one group. 
 * As an application creates SPE threads, the new threads are added to the designated 
 * SPE group. However the total number of SPE threads in a group cannot exceed the 
 * group maximum, which is dependent upon scheduling policy, priority, and 
 * availability of system resources. The spe_group_max function returns the maximum 
 * allowable number of SPE threads for a group.
 * All runnable threads in an SPE group may be gang scheduled for execution. 
 * Gang scheduling permits low-latency interaction among SPE threads in shared-memory 
 * parallel applications.
 * 
 * @param policy Defines the scheduling class for SPE threads in a group. 
 * Accepted values are: 
 * SCHED_RR which indicates real-time round-robin scheduling.
 * SCHED_FIFO which indicates real-time FIFO scheduling.
 * SCHED_OTHER which is used for low priority tasks suitable for filling 
 * otherwise idle SPE cycles. 
 * The real-time scheduling policies SCHED_RR and SCHED_FIFO are available 
 * only to processes with super-user privileges.
 * 
 * @param priority Defines the SPE group's scheduling priority within the policy class.
 * For the real-time policies SCHED_RR and SCHED_FIFO, priority is a value in the range
 * of 1 to 99. For interactive scheduling (SCHED_OTHER) the priority is a value in the 
 * range 0 to 99. The priority for an SPE thread group can be modified with 
 * spe_set_priority, or queried with spe_get_priority.
 * 
 * @param use_events A non-zero value for this parameter allows the application to 
 * receive events for SPE threads in the group. SPE events are conceptually similar 
 * to Linux signals, but differ as follows: SPE events are queued, ensuring that 
 * if multiple events are generated, each is delivered; SPE events are delivered 
 * in the order received; SPE events have associated data, including the type of 
 * event and the SPE thread id. The spe_get_event function can be called to wait 
 * for SPE events.
 * 
 * @return On success, a positive non-zero identifier for a new SPE group is returned. 
 * On error, zero is returned and errno is set to indicate the error.
 * Possible errors include:
 * @retval ENOMEM The SPE group could not be allocated due to lack of system resources.
 * @retval ENOMEM The total number of SPE groups in the system has reached the system 
 * maximum value.
 * @retval EINVAL The requested scheduling policy or priority was invalid.
 * @retval EPERM The process does not have sufficient privileges to create an SPE 
 * group with the requested scheduling policy or priority.
 * @retval ENOSYS The SPE group could not be allocated due to lack of implementation 
 * support for the specified scheduling priority or policy.
 * 
 * @sa spe_create_thread(), spe_group_defaults(), spe_group_max(), spe_get_priority(), 
 * spe_set_priority(), spe_get_policy()
 * 
 */
spe_gid_t
spe_create_group (int policy, int priority, int use_events)
{
	struct group_store *group_store;
	
	/* Sanity: check for vaild policy/priority combination.
	 */
	if (!check_priority(policy, priority))
	{
		errno=EINVAL;
		return NULL;
	}

	DEBUG_PRINTF ("spu_create_group(0x%x, 0x%x, 0x%x)\n",
			                  policy, priority, use_events);
	
	group_store=spe_gid_setup(policy, priority, use_events);
	DEBUG_PRINTF("gid is %p\n", group_store);

	return group_store;
}

/**
 * The spe_destroy_group function marks groups to be destroyed, and destroys them 
 * if they do not contain any SPE threads. If a SPE group cannot be destroyed
 * immediately it will be destroyed after the last SPE thread has ended.
 * @param spe_gid The identifier of a specific SPE group.
 * @return 0 on success, -1 on failure.
 * Possible errors include:
 * @retval ESRCH The specified SPE group could not be found.
 * 
 * @sa spe_create_group, spe_get_group
 */

int
spe_destroy_group(spe_gid_t spe_gid)
{
	struct group_store *gid = spe_gid;

	if ( gid != NULL && !srch_group(gid)) 	{
		errno = ESRCH;
		return -1;
	}
	
	gid->deleteMe=1;
	remove_group_from_groups(gid);
	
	return 0;
}

/**
 * The spe_group_max function returns the maximum number of SPE threads that may be 
 * created for an SPE group, as indicated by gid.
 * The total number of SPE threads in a group cannot exceed the group maximum, 
 * which is dependent upon the group’s scheduling policy, priority, and availability 
 * of system resources.
 * @param spe_gid This is the identifier of the SPE group.
 * @return On success, the maximum number of SPE threads allowed for the SPE group 
 * is return. On error, -1 is returned and errno is set appropriately.
 * Possible errors include:
 * @retval EPERM The calling process does not have privileges to query the SPE group.
 * @retval ESRCH The specified SPE group could not be found.
 * @sa spe_create_group, spe_create_thread
 */
int spe_group_max (spe_gid_t spe_gid)
{
	char	buff[256] = "/sys/devices/system/spu";
	DIR	*dirp;
	int ret = -2;
	struct	dirent	*dptr;
	

	if ( spe_gid != NULL && !srch_group(spe_gid)) 	{
		errno = ESRCH;
		return -1;
	}
	
	DEBUG_PRINTF ("spu_group_max(0x%x)\n", spe_gid);

	 // Count number of SPUs in /sys/devices/system/spu 

	if((dirp=opendir(buff))==NULL)
	{
		fprintf(stderr,"Error opening %s ",buff);
		perror("dirlist");
		return MAX_THREADS_PER_GROUP;
	}
	while(dptr=readdir(dirp))
	{
		ret++;
	}
	closedir(dirp);
	
	return ret;

}

int spe_count_physical_spes(void)
{
	DEBUG_PRINTF ("spe_count_physical_spes()\n");
	return _base_spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, -1);
}


/**
 * spe_create_thread creates a new SPE thread of control that can be executed independently of the calling task. As an application creates SPE threads, the threads are added to the designated SPE group. The total number of SPE threads in a group cannot exceed the group maximum. The spe_group_max function returns the number of SPE threads allowed for the group.
 * @param gid Identifier of the SPE group that the new thread will belong to. SPE group identifiers are returned by spe_create_group. The new SPE thread inherits scheduling attributes from the designated SPE group. If gid is equal to SPE_DEF_GRP (0), then a new group is created with default scheduling attributes, as set by calling spe_group_defaults.
 * 
 * @param handle
 * Indicates the program to be executed on the SPE. This is an opaque pointer to an 
 * SPE ELF image which has already been loaded and mapped into system memory. This 
 * pointer is normally provided as a symbol reference to an SPE ELF executable image 
 * which has been embedded into a PPE ELF object and linked with the calling PPE 
 * program. This pointer can also be established dynamically by loading a shared 
 * library containing an embedded SPE ELF executable, using dlopen(2) and dlsym(2), 
 * or by using the spe_open_image function to load and map a raw SPE ELF executable.
 * @param argp
 * An (optional) pointer to application specific data, and is passed as the second 
 * parameter to the SPE program.
 * @param envp
 * An (optional) pointer to environment specific data, and is passed as the third 
 * parameter to the SPE program.
 * @param mask
 * The processor affinity mask for the new thread. Each bit in the mask enables (1) 
 * or disables (0) thread execution on a cpu. At least one bit in the affinity mask 
 * must be enabled. If equal to -1, the new thread can be scheduled for execution 
 * on any processor. The affinity mask for an SPE thread can be changed by calling 
 * spe_set_affinity, or queried by calling spe_get_affinity.
 * A bit-wise OR of modifiers that are applied when the new thread is created. The 
 * following values are accepted:\n
 * 0 No modifiers are applied\n
 * SPE_CFG_SIGNOTIFY1_OR Configure the Signal Notification 1 Register to be in 
 * 'logical OR' mode instead of the default 'Overwrite' mode.\n
 * SPE_CFG_SIGNOTIFY2_OR Configure the Signal Notification 1 Register to be in 
 * 'logical OR' mode instead of the default 'Overwrite' mode.\n
 * SPE_MAP_PS Request permission for memory-mapped access to the SPE thread’s 
 * problem state area(s). Direct access to problem state is a real-time feature, 
 * and may only be available to programs running with privileged authority 
 * (or in Linux, to processes with access to CAP_RAW_IO; see capget(2) for details).
 * 
 * @param flags SPE_USER_REGS
 * Specifies that the SPE setup registers r3, r4, and r5 are initialized with the 48 bytes pointed to by argp.
 * 
 * @return
 * On success, a positive non-zero identifier of the newly created SPE thread is returned. On error, 0 is returned and errno is set to indicate the error.
 * Possible errors include:
 * @retval ENOMEM The SPE thread could not be allocated due to lack of system resources
 * @retval EINVAL The value passed for mask or flags was invalid. 
 * SPE Thread Management Facilities 5
 * @retval EPERM The process does not have permission to add threads to the designated SPE group, or to use the SPU_MAP_PS setting.
 * @retval ESRCH The SPE group could not be found.
 * @sa spe_create_group
 * spe_get_group spe_get_ls
 * spe_get_ps_area
 * spe_get_threads
 * spe_group_defaults
 * spe_group_max
 * spe_open_image, spe_close_image
 */
speid_t
spe_create_thread (spe_gid_t gid, spe_program_handle_t *handle, void *argp, void *envp, unsigned long mask, int flags)
{
	struct thread_store *thread_store;
	int rc;

	/* Sanity check
	 */
	if (!handle || (( flags & SPE_USER_REGS ) && !argp ))
	{
		errno=EINVAL;
		return NULL;
	}

	if ( gid != NULL && !srch_group(gid))
	{
		return NULL;
	}

	if ( mask == 0 ||  (mask & ((1<<spe_group_max(NULL))-1)) == 0 )
	{
		errno=EINVAL;
		return NULL;
	}
	
	DEBUG_PRINTF ("spe_create_thread(%p, %p, %p, %p, 0x%x, 0x%x)\n",
		      gid, handle, argp, envp, mask, flags);

	if (check_env)
	  env_check();

	thread_store = spe_setup (gid, handle, argp, envp, flags);
	if (!thread_store)
	  {
		  DEBUG_PRINTF ("spe_setup failed\n");
		  errno=EINVAL;
		  return NULL;
	  }


	// remember the affinity mask
	//
	thread_store->mask = mask;

	DEBUG_PRINTF ("pthread_create()\n");
	rc = pthread_create (&thread_store->spe_thread, NULL,
			     spe_thread, thread_store);

	if (rc)
	  {
		  perror ("pthread_create");
		  return NULL;
	  }

	return thread_store;
}

static inline int spe_wait_status(unsigned int spe_status_R)
{
	unsigned int rc, term = 0;

	rc = (spe_status_R & 0xffff0000) >> 16;
	if (spe_status_R & 0x4) {
		/* SPE halted. */
		term = SIGABRT;
	} else if (spe_status_R & 0x20) {
		/* SPE invalid instruction. */
		term = SIGILL;
	} else if (spe_status_R & 0x40) {
		/* SPE invalid ch. */
		term = SIGILL;
	} else if ((rc < SPE_PROGRAM_NORMAL_END) || (rc > 0x20ff)) {
		/* Treat these as invalid stop codes. */
		term = SIGILL;
	} else if (spe_status_R & 0x1) {
		/* Thread killed for undetermined reason. */
		term = SIGKILL;
	}
	/* Return status that can be evaluated with WIFEXITED(), etc. */
	return ((rc & 0xff) << 8) | (term & 0x7f);
}

/**
 * spe_wait suspends execution of the current process until the SPE thread specified 
 * by speid has exited. If the SPE thread has already exited by the time of the 
 * call (a so-called “zombie” SPE thread), then the function returns immediately. 
 * Any system resources used by the SPE thread are freed.
 * @param speid Wait for the SPE thread identified.
 * @param options This parameter is an logical OR of zero or more of the following
 * constants:\n
 * WNOHANG Return immediately if the SPE thread has not exited.\n
 * WUNTRACED Return if the SPE thread is stopped and its status has not been reported.
 * @param status If this value is non-NULL, spe_wait stores the SPE thread’s exit c
 * ode at the address indicated by status. This status can be evaluated with the 
 * following macros. Note: these macros take the status buffer, an int, as a 
 * parameter - not a pointer to the buffer!\n
 * WIFEXITED(status) Is non-zero if the SPE thread exited normally.\n
 * WEXITSTATUS(status) Evaluates to the least significant eight bits of the return 
 * code of the SPE thread which terminated, which may have been set as the argument 
 * to a call to exit() or as the argument for a return statement in the main program. 
 * This macro can only be evaluated if WIFEXITED returned non-zero.\n
 * WIFSIGNALED(status) Returns true if the SPE thread exited because of a signal 
 * which was not caught.
 * WTERMSIG(status) Returns the number of the signal that caused the SPE thread to 
 * terminate. This macro can only be evaluated if WIFSIGNALED returned non-zero.
 * WIFSTOPPED(status) Returns true if the SPE thread which caused the return is 
 * currently stopped; this is only possible if the call was done using WUNTRACED.
 * WSTOPSIG(status) Returns the number of the signal which caused the SPE thread 
 * to stop. This macro can only be evaluated if WIFSTOPPED returned non-zero.
 * @return On success, 0 is returned. A 0 value is returned if WNOHANG was used and 
 * the SPE thread was available. On failure, -1 is returned and errno is set 
 * appropriately.
 * Possible errors include:
 * @retval ESRCH The specified SPE thread could not be found.
 * @retval EINVAL The options parameter is invalid.
 * @retval EFAULT status points to an address that was not contained in the calling 
 * process’s address space.
 * @retval EPERM The calling process does not have permission to wait on the 
 * specified SPE thread.
 * @retval EAGAIN The wait queue was active at the time spe_wait was called, 
 * prohibiting additional waits, so try again.
 * 
 * @sa spe_create_thread
 */
int 
spe_wait(speid_t speid, int *status, int options)
{
	struct thread_store *thread_store = speid;
	void *pthread_rc;
	int rc;

	if (!srch_thread(speid))
	{
		return -1;
	}	
	
	DEBUG_PRINTF("spu_wait(0x%x, 0x%x, 0x%x)\n", speid, *status, options);

	if (options & WNOHANG)
	{
		if (thread_store->thread_status != 99)
		{
			*status = 0;
			return 0;
		}
	}
	
	if (options & WUNTRACED)
	{
		if (thread_store->thread_status == 10 || thread_store->thread_status == 20 )
		{
			*status = 0x4;
		}
	}
	
	DEBUG_PRINTF("waiting for Thread to end.\n");
	rc = pthread_join(thread_store->spe_thread, &pthread_rc);
	if (rc) {
		DEBUG_PRINTF("  pthread_join failed, errno=%d\n", errno);
		return -1;
	}
	if (status) {
		*status = spe_wait_status(thread_store->ret_status);
	}
	spe_cleanup(thread_store);

	DEBUG_PRINTF("Thread ended.\n");
	return rc;
}

/**
 * The spe_kill can be used to send a control signal to an SPE thread.
 * @param speid The signal is delivered to the SPE thread identified.
 * @param sig This indicates the type of control signal to be delivered. 
 * It may be one of the following values:\n
 * SIGKILL Kill the specified SPE thread.\n
 * SIGSTOP Stop execution of the specified SPE thread.\n
 * SIGCONT Resume execution of the specified SPE thread.\n
 * 
 * @return On success, 0 is returned. On error, -1 is returned and errno is set 
 * appropriately.
 * Possible errors include:
 * @retval ENOSYS The spe_kill operation is not supported by the implementation or 
 * environment.
 * @retval EPERM The calling process does not have permission to perform the kill 
 * action for the receiving SPE thread.
 * @retval ESRCH The SPE thread does not exist. Note that a existing SPE thread 
 * might be a zombie, an SPE thread which is already committed termination but 
 * yet had spe_wait called for it.
 * @sa spe_create_thread, spe_wait, kill (2)
 */
int
spe_kill (speid_t speid, int sig)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
	{
		return -1;
	}	
	
	if (sig == SIGCONT)
	{
		int stopped = 0;

		pthread_mutex_lock(&thread_store->event_lock);
		stopped = thread_store->event_pending;
		if ( thread_store->stop || stopped )
		{
			pthread_cond_signal(&thread_store->event_deliver);
		}
		pthread_mutex_unlock(&thread_store->event_lock);
		return 0;
	}
	if (sig == SIGKILL)
	{
		int stopped;
		int ret = 0;

		pthread_mutex_lock(&thread_store->event_lock);
		/* Tell the thread that it is to be killed.*/
		thread_store->killed = 1;

		/* See if it is halted and waiting */
		stopped = thread_store->event_pending;
		if (stopped) {
			pthread_cond_signal(&thread_store->event_deliver);
	        } else {
			ret = pthread_cancel (thread_store->spe_thread);
		}
		pthread_mutex_unlock(&thread_store->event_lock);
		return ret;
	}
	if (sig == SIGSTOP)
	{	
		/* Tell the thread that it is to stop.*/
		thread_store->stop = 1;

		return 0;
	}

	errno = EINVAL;
	return -1;
}

int
validatefd(struct poll_helper *phelper, int pos, int fd)
{
	int i;
	
	for(i=0;i<pos;i++)
	{
		if (phelper[i].retfd == fd)
		{
			DEBUG_PRINTF("Removed double fd. (%i)\n ",fd);
			return 0;
		}
	}
	return 1;
}

/**
 * spe_get_event polls or waits for events that may be generated by threads in an SPE 
 * group.
 * @param pevents This specifies an array of SPE event structures
 * @param nevents This specifies the number of spe_event structures in the pevents 
 * array.
 * @param timeout This specifies the timeout value in milliseconds. A negative value 
 * means an infinite timeout. If none of the events requested (and no error) had 
 * occurred any of the SPE groups, the operating system waits for timeout 
 * milliseconds for one of these events to occur.
 * 
 * @return On success, a positive number is returned, where the number returned 
 * indicates the number of structures which have non-zero revents fields (in other 
 * words, those with events or errors reported). A value of 0 indicates that the 
 * call timed out and no events have been selected. On error, -1 is returned and 
 * errno is set appropriately.\n
 * Possible errors include:
 * @retval EFAULT The array given as a parameter was not contained in the calling 
 * program’s address space.
 * @retval EINVAL No SPE groups have yet been created.
 * @retval EINTR A signal occurred before any requested event.
 * @retval EPERM The current process does not have permission to get SPE events.
 * 
 * @sa spe_create_group, poll
 */
int
spe_get_event(struct spe_event *pevents, int nevents, int timeout)
{
	int i;
	int ret_events = 0;
	int numSPEsToPoll = 0;
	int setupSPEs =0;
	int pollRet = 0;

	struct pollfd *SPEfds;

	struct poll_helper *phelper;
	
	nevents  = 1; // force the number of events returned to 1 for the moment
	pthread_mutex_lock(&grp_list.mutex);

	for (i=0 ; i < nevents ; i++)
	{
		// Clear output fields.
		pevents[i].speid = NULL;
		pevents[i].revents=0;
		pevents[i].data=0;
	}
	
	for (i=0 ; i < nevents ; i++)
	{
		int j;
		struct group_store *group = pevents[i].gid;
		struct grpListElem *elem = group->grp_members;

		DEBUG_PRINTF("spe_get_event():\n");
		DEBUG_PRINTF("  using group  : %p\n", group);
		DEBUG_PRINTF("  with members : %i\n", group->numListSize);

		for ( j=0 ; j < group->numListSize; j++)
		{
			struct thread_store *thread = elem->thread;
		
			DEBUG_PRINTF("  scan member  : %p\n", thread);
			
			// Scan for present events in the thread structures of SPEs
			if (thread->event != 0 && thread->event & pevents[i].events)
			{
				int pipeval,ret;
			
				DEBUG_PRINTF("  has event !  : 0x%04x\n", thread->event);
				
				//Found an event we're looking for.
				ret_events++;

				//Fill out return struct.
				pevents[i].revents = thread->event;
				pevents[i].speid   = thread;
				pevents[i].data    = thread->ev_data;

				//Empty the pipe
				ret=read(thread->spectx->base_private->ev_pipe[0], &pipeval, 4);
				thread->ev_data = 0;
				thread->event = 0;

				pthread_mutex_unlock(&grp_list.mutex);
				return ret_events;
			}
			
			//Decide on what fd's to poll on
			if (pevents[i].events & (SPE_EVENT_MAILBOX))
			{
				numSPEsToPoll++;
			}
			if (pevents[i].events & (SPE_EVENT_TAG_GROUP))
			{
				numSPEsToPoll++;
			}
			if (pevents[i].events & (SPE_EVENT_STOP | SPE_EVENT_DMA_ALIGNMENT |
						SPE_EVENT_SPE_ERROR | SPE_EVENT_SPE_DATA_SEGMENT | 
						SPE_EVENT_SPE_DATA_STORAGE | SPE_EVENT_SPE_TRAPPED | SPE_EVENT_THREAD_EXIT ))
			{
				numSPEsToPoll++;
			
			}
			elem=elem->next;
		}
	}
	
	if(numSPEsToPoll == 0)
	{
		pthread_mutex_unlock(&grp_list.mutex);
		errno=EINVAL;
		return -1;
	}

	if (ret_events > 0)
	{
		//printf("P1\n");
		
		//DEBUG_PRINTF("  returning !  : 0xi\n", ret_events);

		pthread_mutex_unlock(&grp_list.mutex);
		
		return ret_events;
	}
	
	DEBUG_PRINTF("  number of fd : %i\n", numSPEsToPoll);

	SPEfds=malloc (numSPEsToPoll * sizeof(struct pollfd));
	phelper=malloc (numSPEsToPoll * sizeof(struct poll_helper));

	// Set up all necessary fds to poll on and remeber what they are for.
	for (i=0 ; i < nevents ; i++)
	{
		int j;
		struct group_store *group = pevents[i].gid;
		struct grpListElem *elem = group->grp_members;

		for ( j=0 ; j < group->numListSize; j++)
		{
			struct thread_store *thread = elem->thread;
			
			if (pevents[i].events & (SPE_EVENT_MAILBOX))
			{
				SPEfds[setupSPEs].fd=open_if_closed(thread->spectx,FD_IBOX, 0);
				SPEfds[setupSPEs].events=POLLIN;
			
				phelper[setupSPEs].event=i;
				phelper[setupSPEs].thread=thread;
				phelper[setupSPEs].retfd=-1;
				phelper[setupSPEs].type=1;
				//printf("1\n");
				setupSPEs++;
			}
			if (pevents[i].events & (SPE_EVENT_STOP | SPE_EVENT_DMA_ALIGNMENT |
						SPE_EVENT_SPE_ERROR | SPE_EVENT_SPE_DATA_SEGMENT | SPE_EVENT_INVALID_DMA_CMD |
						SPE_EVENT_SPE_DATA_STORAGE | SPE_EVENT_SPE_TRAPPED | SPE_EVENT_THREAD_EXIT ))
			{
				SPEfds[setupSPEs].fd=thread->spectx->base_private->ev_pipe[0];
				SPEfds[setupSPEs].events=POLLIN;
			
				phelper[setupSPEs].event=i;
				phelper[setupSPEs].thread=thread;
				phelper[setupSPEs].retfd=-1;
				phelper[setupSPEs].type=2;
				//printf("2\n");
				setupSPEs++;
			}
			if (pevents[i].events & (SPE_EVENT_TAG_GROUP))
			{
				SPEfds[setupSPEs].fd=open_if_closed(thread->spectx,FD_MFC, 0);
				SPEfds[setupSPEs].events=POLLIN;

				phelper[setupSPEs].event=i;
				phelper[setupSPEs].thread=thread;
				phelper[setupSPEs].retfd=-1;
				phelper[setupSPEs].type=3;
				if (SPEfds[setupSPEs].fd == -1)
					fprintf(stderr, "Warning: spe_get_events: attempting "
						"to wait for tag group without DMA support\n");
				//printf("3\n");
				setupSPEs++;
			}
			elem=elem->next;
		}
	}
	
	if (setupSPEs != numSPEsToPoll)
	{
		DEBUG_PRINTF("ERROR:Thread number mismatch.");
		
		free(SPEfds);
		free(phelper);
		
		pthread_mutex_unlock(&grp_list.mutex);
		errno = EFAULT;
		return -1;
	}

	pthread_mutex_unlock(&grp_list.mutex);
	
	DEBUG_PRINTF("Polling for %i fd events.\n",setupSPEs);

	pollRet = poll(SPEfds, setupSPEs, timeout);
	DEBUG_PRINTF("Poll returned %i events.\n",pollRet);
	
	ret_events = 0;
	
	// Timeout.
	if (pollRet == 0)
	{
		free(SPEfds);
		free(phelper);
		return 0;
	}
	
	
	for (i=0 ; i < setupSPEs ; i++ )
	{
		if (SPEfds[i].revents != 0 && pevents[phelper[i].event].speid == NULL )
		{
			int rc,data;
			
			if (validatefd(phelper,i,SPEfds[i].fd))
			{	
				switch (phelper[i].type) {
				case 1:
					// Read ibox data if present
					rc = read(SPEfds[i].fd, &data, 4);
					if (rc == 4)
					{
						phelper[i].retfd=SPEfds[i].fd;
						pevents[phelper[i].event].data = data;
						pevents[phelper[i].event].speid = phelper[i].thread;
						pevents[phelper[i].event].revents = SPE_EVENT_MAILBOX;
						ret_events++;
					}
					break;
				case 2:
       	                        	// Read pipe data if present
       	                        	rc = read(SPEfds[i].fd, &data, 4);
       	                        	//pevents[phelper[i].event].revents = pevents[phelper[i].event].revents | data;
					phelper[i].retfd=SPEfds[i].fd;
       	                        	pevents[phelper[i].event].revents = data;
					pevents[phelper[i].event].speid = phelper[i].thread;
				       	pevents[phelper[i].event].data = phelper[i].thread->ev_data;
	
				       	phelper[i].thread->ev_data = 0;
				       	phelper[i].thread->event = 0;
					ret_events++;
					break;
				case 3:
					// Read tag group data if present
					rc = read(SPEfds[i].fd, &data, 4);
					if (rc == 4)
					{	
						phelper[i].retfd=SPEfds[i].fd;
						pevents[phelper[i].event].data = data;
						pevents[phelper[i].event].speid = phelper[i].thread;
						pevents[phelper[i].event].revents = SPE_EVENT_TAG_GROUP;
						ret_events++;
					}
					break;
				}
			}
		}
	}
	
	free(SPEfds);
	free(phelper);
	//printf("P2\n");
	return ret_events;
}

/**
 * spe_get_threads returns a list of SPE threads in a group, as indicated by gid, 
 * to the array pointed to by spe_ids.
 * The storage for the spe_ids array must be allocated and managed by the application.
 * Further, the spe_ids array must be large enough to accommodate the current number 
 * of SPE threads in the group. The number of SPE threads in a group can be obtained 
 * by setting the spe_ids parameter to NULL.
 * 
 * @param gid This is the identifier of the SPE group.
 * @param spe_ids This is a pointer to an array of speid_t values that are filled in 
 * with the ids of the SPE threads in the group specified by gid.
 * 
 * @return On success, the number of SPE threads in the group is returned. 
 * On failure, -1 is returned and errno is set appropriately.
 * Possible errors include:
 * @retval EFAULT The spe_ids array was contained within the calling program’s 
 * address space. 
 * @retval EPERM The current process does not have permission to query SPE threads 
 * for this group.
 * @retval ESRCH The specified SPE thread group could not be found.
 * @sa spe_create_group, spe_create_thread
 */
int spe_get_threads(spe_gid_t gid, speid_t *spe_ids)
{
	struct group_store *group = gid;
	int i;

	if ( gid != NULL && !srch_group(gid)) 	{
		errno = ESRCH;
		return -1;
	}
	
	if (!spe_ids)
	{
		return group->numListSize;
	}
	else
	{
		struct grpListElem *elem = group->grp_members;
		
		for(i=0; i < group->numListSize ; i++)
		{
			spe_ids[i] = elem->thread;
			elem=elem->next;
			
		}
	}

	return i;
}
/**
 * The scheduling priority for the SPE thread group, as indicated by gid, is 
 * obtained by calling the spe_get_priority.
 * @param gid The identifier of a specific SPE group.
 * @return On success, spe_get_priority returns a priority value of 0 to 99. On 
 * failure, spe_get_priority returns -1 and sets errno appropriately.
 * Possible errors include:
 * @retval ESRCH The specified SPE thread group could not be found.
 * @sa spe_create_group
 */
int spe_get_priority(spe_gid_t gid)
{
	struct group_store *group_store = gid;

	if ( gid != NULL && !srch_group(gid)) 	{
		errno = ESRCH;
		return -1;
	}
	
	return group_store->priority;
}

/**
 * The scheduling priority for the SPE thread group, as indicated by gid is set by 
 * calling the spe_set_priority function.
 * For the real-time policies SCHED_RR and SCHED_FIFO, priority is a value in the 
 * range of 1 to 99. Only the super-user may modify real-time priorities. 
 * For the interactive policy SCHED_OTHER, priority is a value in the range 0 to 40. 
 * Only the super-user may raise interactive priorities.
 * 
 * @param gid The identifier of a specific SPE group.
 * @param priority Specified the SPE thread group’s scheduling priority within the 
 * group’s scheduling policy class.
 * @return On success, spe_set_priority returns zero. On failure, spe_set_priority 
 * returns -1 and sets errno appropriately.
 * Possible errors include:
 * @retval EINVAL The specified priority value is invalid.
 * @retval EPERM The current process does not have permission to set the specified 
 * SPE thread group priority.
 * @retval ESRCH The specified SPE thread group could not be found.
 * @sa spe_create_group
 */
int spe_set_priority(spe_gid_t gid, int priority)
{
	struct group_store *group_store = gid;

	/* Sanity: check for vaild policy/priority combination.
        */
	if (!check_priority(group_store->policy, priority))
        {
                errno=EINVAL;
                return -1;
        }

	if ( gid != NULL && !srch_group(gid)) 	{
		errno = ESRCH;
		return -1;
	}

	
	group_store->priority=priority;

	/*int pthread_setschedparam(pthread_t target_thread,  int  policy,  const struct sched_param *param);*/
	return 0;
}

int
check_priority(int policy, int priority)
{
	
	switch (policy) {
	case SCHED_RR:
		if (1 > priority || priority > 99)
		{
			return 0;
		}
		break;
	case SCHED_FIFO:
		if (1 > priority || priority > 99)
		{
			return 0;
		}
		break;
	case SCHED_OTHER:
		if (0 > priority || priority > 40)
		{
			return 0;
		}
		break;
	}

	return 1;
}

/**
 * The scheduling policy class for an SPE group is queried by calling the 
 * spe_get_policy function.
 * 
 * @param gid The identifier of a specific SPE group.
 * @return On success, spe_get_policy returns a scheduling policy class value 
 * of SCHED_RR, SCHED_FIFO, or SCHED_OTHER. On failure, spe_get_policy returns -1 
 * and sets errno appropriately.
 * SPE thread group priority.
 * @retval ESRCH The specified SPE thread group could not be found.
 * @sa spe_create_group
 */
int spe_get_policy(spe_gid_t gid)
{
	struct group_store *group_store = gid;

	if ( gid != NULL && !srch_group(gid)) 	{
		errno = ESRCH;
		return -1;
	}
	
	return group_store->policy;
}

/**
 * The spe_get_group function returns the SPE group identifier for the SPE thread, 
 * as indicated by speid.
 * @param speid The identifier of a specific SPE thread.
 * @return The SPE group identifier for an SPE thread, or 0 on failure.
 * Possible errors include:
 * @retval ESRCH The specified SPE thread could not be found.
 * 
 * @sa spe_create_group, spe_get_threads
 */
spe_gid_t
spe_get_group (speid_t speid)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
	{
		return NULL;
	}	
	
	return thread_store->group_id;
}

/**
 * The spe_set_affinity function sets the processor affinity mask for an SPE thread.
 * @param speid Identifier of a specific SPE thread.
 * @param mask The affinity bitmap is represented by the value specified by mask. 
 * The least significant bit corresponds to the first cpu on the system, while the 
 * most significant bit corresponds to the last cpu on the system. A set bit 
 * corresponds to a legally schedulable processor while an unset bit corresponds 
 * to an illegally schedulable processor. In other words, a thread is bound to and 
 * will only run on a cpu whose corresponding bit is set. Usually, all bits in the 
 * mask are set.
 * @return On success, spe_get_affinity and spe_set_affinity return 0. On failure, 
 * -1 is returned and errno is set appropriately. spe_get_affinity returns the 
 * affinity mask in the memory pointed to by the mask parameter. 
 * Possible errors include:
 * @retval EFAULT The supplied memory address for mask was invalid.
 * @retval EINVAL The mask is invalid or cannot be applied.
 * @retval ENOSYS The affinity setting operation is not supported by the i
 * mplementation or environment.
 * @retval ESRCH The specified SPE thread could not be found.
 * @sa spe_create_thread, sched_setaffinity
 */
int
spe_get_affinity( speid_t speid, unsigned long *mask)
{
	int result = 0;

	if (!srch_thread(speid))
	{
		return -1;
	}	

	printf("spe_get_affinity() not implemented in this release.\n");
	
	return result;
}

/**
 * The spe_set_affinity function sets the processor affinity mask for an SPE thread.
 * @param speid Identifier of a specific SPE thread.
 * @param mask The affinity bitmap is represented by the value specified by mask. 
 * The least significant bit corresponds to the first cpu on the system, while the 
 * most significant bit corresponds to the last cpu on the system. A set bit 
 * corresponds to a legally schedulable processor while an unset bit corresponds 
 * to an illegally schedulable processor. In other words, a thread is bound to and 
 * will only run on a cpu whose corresponding bit is set. Usually, all bits in the 
 * mask are set.
 * @return On success, spe_get_affinity and spe_set_affinity return 0. On failure, 
 * -1 is returned and errno is set appropriately. spe_get_affinity returns the 
 * affinity mask in the memory pointed to by the mask parameter. 
 * Possible errors include:
 * @retval EFAULT The supplied memory address for mask was invalid.
 * @retval EINVAL The mask is invalid or cannot be applied.
 * @retval ENOSYS The affinity setting operation is not supported by the i
 * mplementation or environment.
 * @retval ESRCH The specified SPE thread could not be found.
 * @sa spe_create_thread, sched_setaffinity
 */
int
spe_set_affinity(speid_t speid, unsigned long mask)
{
	int result = 0;

	if (!srch_thread(speid))
	{
		return -1;
	}	
	
	printf("spe_set_affinity() not implemented in this release.\n");

	return result;
}

/**
 * spe_group_defaults changes the application defaults for SPE groups. When an 
 * application calls spe_create_thread and designates an SPE group id equal to 
 * SPE_DEF_GRP (0), then a new group is created and the thread is added to the 
 * new group. The group is created with default settings for memory access 
 * privileges and scheduling attributes. By calling spe_group_defaults, the 
 * application can override the settings for these attributes.
 * The initial attribute values for SPE group 0 are defined as follows: 
 * the policy is set to SCHED_OTHER; the priority is set to 0; and spe_events are 
 * disabled.
 * 
 * @param policy This defines the scheduling class. Accepted values are:\n
 * SCHED_RR which indicates real-time round-robin scheduling.\n
 * SCHED_FIFO which indicates real-time FIFO scheduling.\n
 * SCHED_OTHER which is used for low priority tasks suitable 
 * for filling otherwise idle SPE cycles.\n
 * @param priority This defines the default scheduling priority. For the real-time 
 * policies SCHED_RR and SCHED_FIFO, priority is a value in the range of 1 to 99. 
 * For interactive scheduling
 * (SCHED_OTHER) the priority is a value in the range 0 to 99.
 * @param spe_events A non-zero value for this parameter registers the application’s 
 * interest i<n SPE events for the group.
 * @return On success, 0 is returned. On failure, -1 is returned and errno is set 
 * appropriately.
 * Possible errors include:
 * @retval EINVAL The specified policy or priority value is invalid.
 * @sa spe_create_group, spe_create_thread
 */
int 
spe_group_defaults(int policy, int priority, int spe_events)
{
        /* Sanity: check for vaild policy/priority combination.
	 */
	
	if (!check_priority(policy, priority))
	{
		errno=EINVAL;
		return -1;
	}

	default_policy = policy;
	default_priority = priority;
	default_eventmask= spe_events;

	return 0;
}

int spe_set_app_data( speid_t speid, void* data)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
		return -1;
	
	thread_store->app_data = data;
	
	return 0;
}

int spe_get_app_data( speid_t speid, void** p_data)
{
	struct thread_store *thread_store = speid;

	if (!srch_thread(speid))
		return -1;
	
	*p_data = thread_store->app_data;
	
	return 0;
}
