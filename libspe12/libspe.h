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
#ifndef _libspe_h_
#define _libspe_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include "libspe2.h"

typedef void *speid_t;
typedef void *spe_gid_t;

/* Flags for spe_create_thread
 */

#define SPE_USER_REGS		  0x0002	/* 128b user data for r3-5.   */

#define SPE_CFG_SIGNOTIFY1_OR     0x00000010
#define SPE_CFG_SIGNOTIFY2_OR     0x00000020
#define SPE_MAP_PS		  0x00000040

#define SPE_DEF_GRP		  0x00000000

#define MAX_THREADS_PER_GROUP 16

/* spe user context
 */

struct spe_ucontext {				
	unsigned int 	gprs[128][4];
	unsigned int 	fpcr[4];
	unsigned int 	decr;
	unsigned int 	decr_status;
	unsigned int 	npc;
	unsigned int 	tag_mask;
	unsigned int 	event_mask;
	unsigned int 	srr0;
	unsigned int 	_reserved[2];
	void *       	ls;
};

/* spe problem state areas
 */

//enum ps_area { SPE_MSSYNC_AREA, SPE_MFC_COMMAND_AREA, SPE_CONTROL_AREA, SPE_SIG_NOTIFY_1_AREA, SPE_SIG_NOTIFY_2_AREA };


/* SIGSPE maps to SIGURG 
*/

#define SIGSPE SIGURG

/* spe_events  
 */

#define SPE_EVENT_MAILBOX		0x0001	/*Interrupting mailbox data   */
#define SPE_EVENT_STOP			0x0002	/*SPE 'stop-and-signal data'  */
//#define SPE_EVENT_TAG_GROUP		0x0004	/*Tag group complete data     */
#define SPE_EVENT_DMA_ALIGNMENT		0x0008	/*A DMA alignment error       */
#define SPE_EVENT_SPE_ERROR		0x0010	/*An illegal instruction error*/
#define SPE_EVENT_SPE_DATA_SEGMENT	0x0020	/*A DMA segmentation error    */
#define SPE_EVENT_SPE_DATA_STORAGE	0x0040	/*A DMA storage error         */
#define SPE_EVENT_SPE_TRAPPED		0x0080	/*SPE 'halt' instruction      */
#define SPE_EVENT_THREAD_EXIT		0x0100	/*A thread has exited         */
#define SPE_EVENT_ERR			0x0200	/*An error occurred           */
#define SPE_EVENT_NVAL			0x0400	/*Invalid request             */
#define SPE_EVENT_INVALID_DMA_CMD	0x0800	/*Invalid MFC/DMA command     */

struct spe_event {
	spe_gid_t	gid;		/* input, SPE group id 		*/
	int		events;		/* input, requested event mask 	*/
	int		revents;	/* output, returned events 	*/
	speid_t		speid;		/* output, returned speid 	*/
	unsigned long 	data;		/* output, returned data	*/
};

/* Signal Targets 
 */

#define SPE_SIG_NOTIFY_REG_1		0x0001
#define SPE_SIG_NOTIFY_REG_2		0x0002

/* APIs for SPE threads.
 */
extern spe_gid_t spe_create_group (int policy, int priority, int spe_events);
extern int spe_group_max (spe_gid_t spe_gid);
extern int spe_count_physical_spes(void);

extern speid_t spe_create_thread (spe_gid_t gid, spe_program_handle_t *handle,
				  void *argp, void *envp,
				  unsigned long mask, int flags);

extern int spe_wait (speid_t speid, int *status, int options);
extern int spe_get_event(struct spe_event *pevents, int nevents, int timeout);
extern int spe_kill (speid_t speid, int sig);
extern int spe_get_priority(spe_gid_t gid);
extern int spe_set_priority(spe_gid_t gid, int priority);
extern int spe_get_policy(spe_gid_t gid);
extern spe_gid_t spe_get_group (speid_t speid);
extern void *spe_get_ls(speid_t speid);
extern int spe_group_defaults(int policy, int priority, int spe_events);
extern int spe_get_threads(spe_gid_t gid, speid_t *spe_ids);
extern int spe_destroy_group(spe_gid_t spe_gid);

/* Currently without implementation or support
 */
extern int spe_get_affinity( speid_t speid, unsigned long *mask);
extern int spe_set_affinity(speid_t speid, unsigned long mask);
extern int spe_get_context(speid_t speid, struct spe_ucontext *uc);
extern int spe_set_context(speid_t speid, struct spe_ucontext *uc);

/* APIs for loading SPE images
 */
extern spe_program_handle_t *spe_open_image(const char *filename);
extern int spe_close_image(spe_program_handle_t *program_handle);

/* ===========================================================================
 * mfc function wrappers in spe library are defined here.
 */

/* FUNCTION:    spe_write_in_mbox (speid, val)
 *
 * Send value to the PPE->SPE mailbox WITHOUT first checking to make sure the
 * PPE->SPE mailbox queue is NOT FULL.  If the queue is full, this would result
 * in an OVERWRITE of the last mailbox value sent.
 */
extern int spe_write_in_mbox (speid_t speid ,unsigned int data);

/* FUNCTION:    spe_stat_in_mbox (speid)
 *
 * Returns the status of the PE->SPE mailbox.
 */
extern int spe_stat_in_mbox (speid_t speid);

/* FUNCTION:    spe_read_out_mbox (speid)
 *
 * Return the contents of the SPE->PPE mailbox without first checking to see
 * if a new mailbox value is present.  This allows for re-read of this
 * register.
 */
extern unsigned int spe_read_out_mbox (speid_t speid);

/* FUNCTION:    spe_stat_out_mbox (speid)
 *
 * Returns the status of the SPE->PE mailbox.
 */
extern int spe_stat_out_mbox (speid_t speid);

/* FUNCTION:    spe_stat_out_intr_mbox (speid)
 *
 * Returns the status of the SPE->PPE interrupting mailbox.
 */
extern int spe_stat_out_intr_mbox (speid_t speid);

/* FUNCTION:    spe_write_signal (speid, signal_reg, data)
 *
 * Write the data word to the specified SPE signal notification register.
 */
extern int spe_write_signal (speid_t speid, unsigned int signal_reg, unsigned int data);

/* FUNCTION:	spe_mfc_put (speid, src, dst, size, tag, tid, rid)
 *		spe_mfc_putf(speid, src, dst, size, tag, tid, rid)
 *		spe_mfc_putb(speid, src, dst, size, tag, tid, rid)
 *
 * initiate data transfer from local storage to process memory,
 * optionally including a fence (putf) or barrier (putb)
 */
extern int spe_mfc_put (speid_t spe, unsigned src, void *dst,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);
extern int spe_mfc_putf(speid_t spe, unsigned src, void *dst,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);
extern int spe_mfc_putb(speid_t spe, unsigned src, void *dst,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);

/* FUNCTION:	spe_mfc_get (speid, dst, src, size, tag, tid, rid)
 *		spe_mfc_getf(speid, dst, src, size, tag, tid, rid)
 *		spe_mfc_getb(speid, dst, src, size, tag, tid, rid)
 *
 * initiate data transfer from process memory to local storage
 * optionally including a fence (getf) or barrier (getb)
 */
extern int spe_mfc_get (speid_t spe, unsigned dst, void *src,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);
extern int spe_mfc_getf(speid_t spe, unsigned dst, void *src,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);
extern int spe_mfc_getb(speid_t spe, unsigned dst, void *src,
			unsigned size, unsigned short tag,
			unsigned char tid, unsigned char rid);

/* FUNCTION:	spe_mfc_read_tag_status_all(speid, mask);
 *              spe_mfc_read_tag_status_any(speid, mask);
 *              spe_mfc_read_tag_status_immediate(speid, mask);
 *
 * read the mfc tag status register.
 */

extern int spe_mfc_read_tag_status_all(speid_t speid, unsigned int mask);

extern int spe_mfc_read_tag_status_any(speid_t speid, unsigned int mask);

extern int spe_mfc_read_tag_status_immediate(speid_t speid, unsigned int mask);


/* FUNCTION:	spe_get_ps_area(speid, ps_area)
 *
 * returns a pointer to the problem state area specified by ps_area.
 */

extern void *spe_get_ps_area (speid_t speid, enum ps_area);

/* INOFFICIAL FUNCTION:	__spe_get_context_fd(speid)
 *
 * returns the file descriptor for the spe context.
 */
extern int __spe_get_context_fd(speid_t speid);

#ifdef __cplusplus
}
#endif

#endif

