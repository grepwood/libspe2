/*
 * libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS 
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
 


#ifndef _SPEEVENT_H_
#define _SPEEVENT_H_

#include "spebase.h"

/* private constants */
enum __spe_event_types {
  __SPE_EVENT_OUT_INTR_MBOX, __SPE_EVENT_IN_MBOX,
  __SPE_EVENT_TAG_GROUP, __SPE_EVENT_SPE_STOPPED,
  __NUM_SPE_EVENT_TYPES
};

/* private types */
typedef struct spe_context_event_priv
{
  pthread_mutex_t lock;
  pthread_mutex_t stop_event_read_lock;
  int stop_event_pipe[2];
  spe_event_unit_t events[__NUM_SPE_EVENT_TYPES];
} spe_context_event_priv_t, *spe_context_event_priv_ptr_t;


int _event_spe_stop_info_read (spe_context_ptr_t spe, spe_stop_info_t *stopinfo);

/* 
 * spe_event_handler_create
 */
 
spe_event_handler_ptr_t _event_spe_event_handler_create(void);

/*
 * spe_event_handler_destroy
 */
 
int _event_spe_event_handler_destroy (spe_event_handler_ptr_t evhandler);

/*
 * spe_event_handler_register
 */
 
int _event_spe_event_handler_register(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event);

/*
 * spe_event_handler_deregister
 */

int _event_spe_event_handler_deregister(spe_event_handler_ptr_t evhandler, spe_event_unit_t *event);

/*
 * spe_event_wait
 */
 
int _event_spe_event_wait(spe_event_handler_ptr_t evhandler, spe_event_unit_t *events, int max_events, int timeout);

int _event_spe_context_finalize(spe_context_ptr_t spe);

struct spe_context_event_priv * _event_spe_context_initialize(spe_context_ptr_t spe);

int _event_spe_context_run	(spe_context_ptr_t spe, unsigned int *entry, unsigned int runflags, void *argp, void *envp, spe_stop_info_t *stopinfo);

void _event_spe_context_lock(spe_context_ptr_t spe);
void _event_spe_context_unlock(spe_context_ptr_t spe);

#endif /*SPEEVENT_H_*/
