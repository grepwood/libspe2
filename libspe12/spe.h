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
#ifndef _spe_h_
#define _spe_h_

/* Definitions
 */
#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); }
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#else
#define DEBUG_PRINTF(fmt, args...)
#endif


struct thread_store
{
	/* Note: The debugger accesses this data structure, and assumes it
	   starts out with an spe_program_handle_t identifying the SPE
	   executable running within this SPE thread.  Everything below
	   is private to libspe.  */
	spe_program_handle_t handle;

	pthread_t 		spe_thread;
	pthread_cond_t 	event_deliver;
	pthread_mutex_t event_lock;
    
	int 			policy, priority, affinity_mask, spe_flags,
					npc, ret_status,event;
	spe_gid_t 		group_id;
	
	unsigned long	mask;
	unsigned int    flags;
	unsigned long 	ev_data;
	int 		event_pending;
	int 		killed;
	int 		stop;
	int		thread_status;
	
	/* Below here comes the wrapper code
	 * */
	
	int ev_pipe[2];
	
	spe_context_ptr_t spectx;
	void 	*argp; 
	void 	*envp;
	
	void 	*app_data;
	
};

/*Group members list
 */

struct grpListElem
{
        struct grpListElem *next;
        struct thread_store *thread;
};

struct group_store
{
        int 		policy, priority, use_events;
        struct 		grpListElem *grp_members;
        int 		numListSize;
        int 		deleteMe;
};
/*Groups List
 */
struct grpsElem
{
        struct grpsElem 	*next;
        struct group_store 	*group;
};

struct group_list
{
        struct grpsElem *list_start;
	pthread_mutex_t	mutex;
        int 		numListSize;
};

struct spe_ctx {
	unsigned int npc;
	unsigned int status;
	unsigned long reg[128][4];		// size error for LP64
	void *ls;
};

struct poll_helper {
        int event;
        struct thread_store *thread;
        int type;
        int retfd;
};

/* Low-level SPE execution API.
 */


extern struct thread_store *srch_thread(struct thread_store *thread);
extern struct group_store *srch_group(struct group_store *group);

extern int add_group_to_groups(struct group_store *gid);
extern int add_thread_to_group(struct group_store *gid, struct thread_store *thread);
extern int remove_thread_from_group(struct group_store *gid, struct thread_store *thread);

extern void *spe_gid_setup(int policy, int priority, int use_events);
extern void *spe_setup (spe_gid_t gid, spe_program_handle_t *handle, void *argp, void *envp, int flags);
extern void spe_cleanup (void *spe);
extern int do_spe_run (void *spe);
extern unsigned int set_npc (void *spe, unsigned int npc);
extern int validatefd(struct poll_helper *phelper, int pos, int fd);

extern void register_handler(void * handler, unsigned int callnum );
extern int check_priority(int policy, int priority);
extern int remove_group_from_groups(struct group_store *gid);

void env_check(void);
void * spe_thread (void *ptr);

/*
 * For testing purposes only 
 */

#endif
