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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "spebase.h"
#include "lib_builtin.h"
#include "default_c99_handler.h"
#include "default_posix1_handler.h"
#include "default_libea_handler.h"

#define HANDLER_IDX(x) (x & 0xff)

/*
 * Default SPE library call handlers for 21xx stop-and-signal.
 */
static void *handlers[256] = {
	[HANDLER_IDX(SPE_C99_CLASS)]	= _base_spe_default_c99_handler,
	[HANDLER_IDX(SPE_POSIX1_CLASS)]	= _base_spe_default_posix1_handler,
	[HANDLER_IDX(SPE_LIBEA_CLASS)]	= _base_spe_default_libea_handler
};

int _base_spe_callback_handler_register(void * handler, unsigned int callnum, unsigned int mode)
{
	errno = 0;

	if (callnum > MAX_CALLNUM) {
		errno = EINVAL;
		return -1;
	}
	
	switch(mode){
	case SPE_CALLBACK_NEW:
		if (callnum < RESERVED) {
			errno = EACCES;
			return -1;
		}
		if (handlers[callnum] != NULL) {
			errno = EACCES;
			return -1;
		}
		handlers[callnum] = handler;
		break;

	case SPE_CALLBACK_UPDATE:
		if (handlers[callnum] == NULL) {
			errno = ESRCH;
			return -1;
		}
		handlers[callnum] = handler;
		break;
	default:
		errno = EINVAL;
		return -1;
		break;
	}
	return 0;

}

int _base_spe_callback_handler_deregister(unsigned int callnum )
{
	errno = 0;
	if (callnum > MAX_CALLNUM) {
		errno = EINVAL;
		return -1;
	}
	if (callnum < RESERVED) {
		errno = EACCES;
		return -1;
	}
	if (handlers[callnum] == NULL) {
		errno = ESRCH;
		return -1;
	}

	handlers[callnum] = NULL;
	return 0;
}

void * _base_spe_callback_handler_query(unsigned int callnum )
{
	errno = 0;

	if (callnum > MAX_CALLNUM) {
		errno = EINVAL;
		return NULL;
	}
	if (handlers[callnum] == NULL) {
		errno = ESRCH;
		return NULL;
	}
	return handlers[callnum];
}

int _base_spe_handle_library_callback(struct spe_context *spe, int callnum,
				      unsigned int npc)
{
	int (*handler)(void *, unsigned int);
	int rc;
	
	errno = 0;
	if (!handlers[callnum]) {
		DEBUG_PRINTF ("No SPE library handler registered for this call.\n");
		errno=ENOSYS;
		return -1;
	}

	handler=handlers[callnum];
	
	/* For emulated isolation mode, position the
	 * npc so that the buffer for the PPE-assisted
	 * library calls can be accessed. */
	if (spe->base_private->flags & SPE_ISOLATE_EMULATE)
		npc = SPE_EMULATE_PARAM_BUFFER;

	rc = handler(spe->base_private->mem_mmap_base, npc);
	if (rc) {
		DEBUG_PRINTF ("SPE library call unsupported.\n");
		errno=ENOSYS;
		return rc;
	}
	return 0;
}

