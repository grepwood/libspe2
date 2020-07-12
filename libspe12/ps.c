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

#define _GNU_SOURCE
#include <errno.h>
#include <libspe.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>

#include "spe.h"
#include "spebase.h"

#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); }
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#else
#define DEBUG_PRINTF(fmt, args...)
#endif


void *spe_get_ps_area(speid_t speid, enum ps_area area)
{
	void *base_addr;
	struct thread_store *spe = speid;

	if (!srch_thread(speid)){
		errno = ESRCH;
		return NULL;
	}

	if (!(spe->flags & SPE_MAP_PS)) {
		errno = EACCES;
		return NULL;
	}

	if ( area == SPE_MSSYNC_AREA ) {
		 		 base_addr = _base_spe_ps_area_get(spe->spectx, SPE_MSSYNC_AREA);
		 		 if (base_addr == NULL) {
		 		 		 perror("spe_map_ps_area: internal error. cant map mss file.");
		 		 		 errno = EIO;
		 		 		 return NULL;
		 		 }
		 		 return base_addr;
	} else if ( area == SPE_MFC_COMMAND_AREA ) {
		base_addr = _base_spe_ps_area_get(spe->spectx, SPE_MFC_COMMAND_AREA);
		if (base_addr == NULL) {
                        perror("spe_map_ps_area: internal error. cant map mfc file.");
                        errno = EIO;
			return NULL;
		}
		 		 return base_addr ;
	} else if ( area == SPE_CONTROL_AREA ) {
		base_addr = _base_spe_ps_area_get(spe->spectx, SPE_CONTROL_AREA);
		if (base_addr == NULL) {
                        perror("spe_map_ps_area: internal error. cant map control file.");
                        errno = EIO;
			return NULL;
		}
		 		 return base_addr ;
	} else if ( area == SPE_SIG_NOTIFY_1_AREA ) {
		base_addr = _base_spe_ps_area_get(spe->spectx, SPE_SIG_NOTIFY_1_AREA);
		if (base_addr == NULL) {
                        perror("spe_map_ps_area: internal error. cant map signal1 file.");
                        errno = EIO;
			return NULL;
		}
		 		 return base_addr; 
	} else if ( area == SPE_SIG_NOTIFY_2_AREA ) {
		base_addr = _base_spe_ps_area_get(spe->spectx, SPE_SIG_NOTIFY_2_AREA);
		if (base_addr == NULL) {
                        perror("spe_map_ps_area: internal error. cant map signal2 file.");
                        errno = EIO;
			return NULL;
		}
		 		 return base_addr; 
	}
	
	perror("spe_map_ps_area: Unsupported call of spe_map_ps_area.");
	errno = EINVAL;
	return NULL;
}
