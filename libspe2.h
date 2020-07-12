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
  * libspe2.h contains the public API funtions
  */

#ifndef libspe2_h
#define libspe2_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdio.h>

/*
 * GLOBAL SYMBOLS AND STRUCTURES
 * libspe2-types header file
 */
#include "libspe2-types.h"


/*
 *---------------------------------------------------------
 * 
 * API functions
 * 
 *---------------------------------------------------------
 */

/*
 * spe_context_create
 */
spe_context_ptr_t spe_context_create(unsigned int flags, spe_gang_context_ptr_t gang);

/*
 * spe_context_create
 */
spe_context_ptr_t spe_context_create_affinity(unsigned int flags, spe_context_ptr_t affinity_neighbor, spe_gang_context_ptr_t gang);

/*
 * spe_context_destroy
 */
int spe_context_destroy (spe_context_ptr_t spe);

/*
 * spe_gang_context_create
 */
spe_gang_context_ptr_t spe_gang_context_create (unsigned int flags);

/*
 * spe_gang_context_destroy
 */
int spe_gang_context_destroy (spe_gang_context_ptr_t gang);

/*
 * spe_image_open
 */
spe_program_handle_t * spe_image_open (const char *filename);

/*
 * spe_image_close
 */
int spe_image_close (spe_program_handle_t *program);

/*
 * spe_load_program
 */
int spe_program_load (spe_context_ptr_t spe, spe_program_handle_t *program);

/*
 * spe_context_run
 */
int spe_context_run	(spe_context_ptr_t spe, unsigned int *entry, unsigned int runflags, void *argp, void *envp, spe_stop_info_t *stopinfo);

/*
 * spe_stop_info_read
 */
int spe_stop_info_read (spe_context_ptr_t spe, spe_stop_info_t *stopinfo);

/* 
 * spe_event_handler_create
 */
spe_event_handler_ptr_t spe_event_handler_create(void);

/*
 * spe_event_handler_destroy
 */
int spe_event_handler_destroy (spe_event_handler_ptr_t evhandler);

/*
 * spe_event_handler_register
 */
int spe_event_handler_register(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event);

/*
 * spe_event_handler_deregister
 */
int spe_event_handler_deregister(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event);

/*
 * spe_event_wait
 */
int spe_event_wait(spe_event_handler_ptr_t evhandler, spe_event_unit_t *events, int max_events, int timeout);

/* 
 * MFCIO Proxy Commands
 */
int spe_mfcio_put (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

int spe_mfcio_putb (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

int spe_mfcio_putf (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

int spe_mfcio_get (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

int spe_mfcio_getb (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

int spe_mfcio_getf (spe_context_ptr_t spe, unsigned int ls, void *ea, unsigned int size, unsigned int tag, unsigned int tid, unsigned int rid);

/*
 * MFCIO Tag Group Completion
 */
int spe_mfcio_tag_status_read(spe_context_ptr_t spe, unsigned int mask, unsigned int behavior, unsigned int *tag_status);

/*
 * SPE Mailbox Facility
 */ 
int spe_out_mbox_read (spe_context_ptr_t spe, unsigned int *mbox_data, int count);

int spe_out_mbox_status (spe_context_ptr_t spe);

int spe_in_mbox_write (spe_context_ptr_t spe, unsigned int *mbox_data, int count, unsigned int behavior);

int spe_in_mbox_status (spe_context_ptr_t spe);

int spe_out_intr_mbox_read (spe_context_ptr_t spe, unsigned int *mbox_data, int count, unsigned int behavior);

int spe_out_intr_mbox_status (spe_context_ptr_t spe);

/*
 * SPU SIgnal Notification Facility
 */
int spe_signal_write (spe_context_ptr_t spe, unsigned int signal_reg, unsigned int data);

/*
 * spe_ls_area_get
 */
void * spe_ls_area_get (spe_context_ptr_t spe);

/*
 * spe_ls_size_get
 */
int spe_ls_size_get (spe_context_ptr_t spe);

/*
 * spe_ps_area_get
 */
void * spe_ps_area_get (spe_context_ptr_t spe, enum ps_area area);

/*
 * spe_callback_handler_register
 */
int spe_callback_handler_register (void *handler, unsigned int callnum, unsigned int mode);

/*
 * spe_callback_handler_deregister
 */
int spe_callback_handler_deregister (unsigned int callnum);

/*
 * spe_callback_handler_query
 */
void * spe_callback_handler_query(unsigned int callnum);


/*
 * spe_info_get
 */
int spe_cpu_info_get(int info_requested, int cpu_node); 


#ifdef __cplusplus
}
#endif


#endif /*libspe2_h*/
