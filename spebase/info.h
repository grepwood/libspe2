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

#ifndef _info_h_
#define _info_h_

#include "spebase.h"

#define THREADS_PER_BE 2 

int _base_spe_count_physical_cpus(int cpu_node);
int _base_spe_count_physical_spes(int cpu_node);
int _base_spe_count_usable_spes(int cpu_node);
int _base_spe_read_cpu_type(int cpu_node);

/* Here is a list of edp capable PVRs
 * Known non-EDP are: 0x0070 0501 ( PS3, QS20, QS21 )
 * Known EPD capable: 0x0070 3000 ( QS22 )
 */
unsigned long pvr_list_edp[] = {0x00703000, 0};

#endif
