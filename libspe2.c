/*
 * SPE Runtime Management Library, Version 2.0
 * This library constitutes the standardized low-level application 
 * programming interface for application access to the Cell Broadband 
 * Engineís Synergistic Processing Elements (SPEs).
 * 
 * The usage of this library requires that the application programmer 
 * is familiar with the CBE architecture as described in ìCell 
 * Broadband Engine Architecture, Version 1.0î.
 * 
 * Copyright (C) 2006 IBM Corp. 
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
 /* 
  * libspe2.ch contains the implementation of public API funtions
  */


#include "libspe2.h"
#include "spebase.h"
#include "speevent.h"

/*
 * spe_context_create
 */
spe_context_ptr_t spe_context_create(unsigned int flags, spe_gang_context_ptr_t gang)
{
	spe_context_ptr_t spe = _base_spe_context_create(flags, gang, NULL);
	if ( spe == NULL ) {
		return NULL;	
	} else {
		if ( flags & SPE_EVENTS_ENABLE ) { /* initialize event handling */
			spe->event_private = _event_spe_context_initialize(spe);
			if ( spe->event_private == NULL ) { /* error initializing event handling */
				_base_spe_context_destroy(spe);
				return NULL;
			}
		} else {
			spe->event_private = NULL;
		}
	}	
	return spe; 
}

/*
 * spe_context_create_affinity
 */
spe_context_ptr_t spe_context_create_affinity(unsigned int flags, spe_context_ptr_t affinity_neighbor, spe_gang_context_ptr_t gang)
{
	spe_context_ptr_t spe = _base_spe_context_create(flags, gang, affinity_neighbor);
	if ( spe == NULL ) {
		return NULL;
	} else {
		if ( flags & SPE_EVENTS_ENABLE ) { /* initialize event handling */
			spe->event_private = _event_spe_context_initialize(spe);
			if ( spe->event_private == NULL ) { /* error initializing event handling */
				_base_spe_context_destroy(spe);
				return NULL;
			}
		} else {
			spe->event_private = NULL;
		}
	}
	return spe;
}

/*
 * spe_context_destroy
 */

int spe_context_destroy (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	if ( spe->event_private != NULL ) {
		int ret = _event_spe_context_finalize(spe);
		if (ret) { /* error releasing event-related resources */
			return ret;
		}
	}
	return _base_spe_context_destroy(spe);
}

/*
 * spe_gang_context_create
 */
 
spe_gang_context_ptr_t spe_gang_context_create (unsigned int flags)
{
	return _base_spe_gang_context_create(flags); 	
}

/*
 * spe_gang_context_destroy
 */
 
int spe_gang_context_destroy (spe_gang_context_ptr_t gang)
{
	if (gang == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_gang_context_destroy (gang);
}

/*
 * spe_image_open
 */
 
spe_program_handle_t * spe_image_open (const char *filename)
{
	return _base_spe_image_open(filename);
}

/*
 * spe_image_close
 */
 
int spe_image_close (spe_program_handle_t *program)
{
	return _base_spe_image_close(program);
}

/*
 * spe_load_program
 */
 
int spe_program_load (spe_context_ptr_t spe, spe_program_handle_t *program)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	if (program == NULL ) {
		errno = EINVAL;
		return -1;
	}
	return _base_spe_program_load(spe, program);
}

/*
 * spe_context_run
 */
 
int spe_context_run	(spe_context_ptr_t spe, unsigned int *entry, unsigned int runflags, void *argp, void *envp, spe_stop_info_t *stopinfo)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	if ( spe->event_private != NULL ) {
		return _event_spe_context_run(spe, entry, runflags, argp, envp, stopinfo);
	} else {
		return _base_spe_context_run(spe, entry, runflags, argp, envp, stopinfo);
	}
}

/*
 * spe_stop_info_read
 */
 
int spe_stop_info_read (spe_context_ptr_t spe, spe_stop_info_t *stopinfo)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	if (spe->event_private == NULL) {
		errno = ENOTSUP;
		return -1;
	} else {
		return _event_spe_stop_info_read (spe, stopinfo);
	}
}

/* 
 * spe_event_handler_create
 */
 
spe_event_handler_ptr_t spe_event_handler_create(void)
{
	return _event_spe_event_handler_create();
}

/*
 * spe_event_handler_destroy
 */
 
int spe_event_handler_destroy (spe_event_handler_ptr_t evhandler)
{
	return _event_spe_event_handler_destroy(evhandler);
}

/*
 * spe_event_handler_register
 */
 
int spe_event_handler_register(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event)
{
	return _event_spe_event_handler_register(evhandler, event);
}

/*
 * spe_event_handler_deregister
 */

int spe_event_handler_deregister(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event)
{
	return _event_spe_event_handler_deregister(evhandler, event);
}

/*
 * spe_event_wait
 */
 
int spe_event_wait(spe_event_handler_ptr_t evhandler, spe_event_unit_t *events, int max_events, int timeout)
{
	return _event_spe_event_wait(evhandler, events, max_events, timeout);
}

/* 
 * MFCIO Proxy Commands
 */
 
int spe_mfcio_put (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_put(spe, ls, ea, size, tag, tid, rid);
}

int spe_mfcio_putb (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_putb(spe, ls, ea, size, tag, tid, rid);
}

int spe_mfcio_putf (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_putf(spe, ls, ea, size, tag, tid, rid);
}

int spe_mfcio_get (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_get(spe, ls, ea, size, tag, tid, rid);
}

int spe_mfcio_getb (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_getb(spe, ls, ea, size, tag, tid, rid);
}

int spe_mfcio_getf (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_getf(spe, ls, ea, size, tag, tid, rid);
}

/*
 * MFCIO Tag Group Completion
 */

int spe_mfcio_tag_status_read(spe_context_ptr_t spe, unsigned int mask, unsigned int behavior, unsigned int *tag_status)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mfcio_tag_status_read(spe, mask, behavior, tag_status);
}

/*
 * SPE Mailbox Facility
 */
 
int spe_out_mbox_read (spe_context_ptr_t spe, unsigned int *mbox_data, int count) 
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_out_mbox_read(spe, mbox_data, count);
}

int spe_out_mbox_status (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_out_mbox_status(spe);
}

int spe_in_mbox_write (spe_context_ptr_t spe, unsigned int *mbox_data, int count, unsigned int behavior)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_in_mbox_write(spe, mbox_data, count, behavior);
}

int spe_in_mbox_status (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_in_mbox_status(spe);
}

int spe_out_intr_mbox_read (spe_context_ptr_t spe, unsigned int *mbox_data, int count, unsigned int behavior)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_out_intr_mbox_read(spe, mbox_data, count, behavior);
}

int spe_out_intr_mbox_status (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_out_intr_mbox_status(spe);
}

/*
 * Multisource Sync Facility
 */

int spe_mssync_start(spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mssync_start(spe);
}

int spe_mssync_status(spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_mssync_status(spe);
}


/*
 * SPU SIgnal Notification Facility
 */

int spe_signal_write (spe_context_ptr_t spe, unsigned int signal_reg, unsigned int data)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_signal_write(spe, signal_reg, data);
}

/*
 * spe_ls_area_get
 */
 
void * spe_ls_area_get (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return NULL;
	}
	return _base_spe_ls_area_get (spe);
}

/*
 * spe_ls_size_get
 */
 
int spe_ls_size_get (spe_context_ptr_t spe)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return -1;
	}
	return _base_spe_ls_size_get(spe);
}

/*
 * spe_ps_area_get
 */
 
void * spe_ps_area_get (spe_context_ptr_t spe, enum ps_area area)
{
	if (spe == NULL ) {
		errno = ESRCH;
		return NULL;
	}
	return _base_spe_ps_area_get(spe, area);
}

/*
 * spe_callback_handler_register
 */
 
int spe_callback_handler_register (void *handler, unsigned int callnum, unsigned int mode)
{
	return _base_spe_callback_handler_register(handler, callnum, mode);
}

/*
 * spe_callback_handler_deregister
 */
 
int spe_callback_handler_deregister (unsigned int callnum)
{
	return _base_spe_callback_handler_deregister(callnum);
}

/*
 * spe_callback_handler_query
 */
void * spe_callback_handler_query(unsigned int callnum) 
{
	return _base_spe_callback_handler_query(callnum);
}

/*
 * spe_info_get
 */
int spe_cpu_info_get(int info_requested, int cpu_node) 
{
	return _base_spe_cpu_info_get(info_requested, cpu_node);
}

#ifdef __cplusplus
}
#endif


