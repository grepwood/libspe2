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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


#include "info.h"

/*
 * For the moment, we count the numbers of cpus and devide by 2.
 */ 
int _base_spe_count_physical_cpus(int cpu_node)
{
	const char    *buff = "/sys/devices/system/cpu";
	DIR     *dirp;
	int ret = 0;
	struct  dirent  *dptr;

	DEBUG_PRINTF ("spe_count_physical_cpus()\n");
	
	// make sure, cpu_node is in the correct range
	if (cpu_node != -1) {
		errno = EINVAL;
		return -1;
	}

	// Count number of CPUs in /sys/devices/system/cpu
	if((dirp=opendir(buff))==NULL) {
		fprintf(stderr,"Error opening %s ",buff);
		perror("dirlist");
		errno = EINVAL;
		return -1;
	}
    while((dptr=readdir(dirp))) {
		if(strncmp("cpu",dptr->d_name,3) == 0)
			ret++;
	}
	closedir(dirp);
	return ret/THREADS_PER_BE; 
}

/*
 * For the moment, we use all spes which are controlled by linux
 */
int _base_spe_count_usable_spes(int cpu_node)
{
	return _base_spe_count_physical_spes(cpu_node); // FIXME
}

/*
 * For the moment, we assume all SPEs are evenly distributed over the 
 * physical cpsus.
 */
int _base_spe_count_physical_spes(int cpu_node)
{
	const char	*buff = "/sys/devices/system/spu";
	DIR	*dirp;
	int ret = -2;
	struct	dirent	*dptr;
	int no_of_bes;

	DEBUG_PRINTF ("spe_count_physical_spes()\n");

	// make sure, cpu_node is in the correct range
	no_of_bes = _base_spe_count_physical_cpus(-1);
	if (cpu_node < -1 || cpu_node >= no_of_bes ) {
		errno = EINVAL;
		return -1;
	}

	// Count number of SPUs in /sys/devices/system/spu
	if((dirp=opendir(buff))==NULL) {
		fprintf(stderr,"Error opening %s ",buff);
		perror("dirlist");
		errno = EINVAL;
		return -1;
	}
    while((dptr=readdir(dirp))) {
		ret++;
	}
	closedir(dirp);

	if(cpu_node != -1) ret /= no_of_bes; // FIXME
	return ret;
}


int _base_spe_cpu_info_get(int info_requested, int cpu_node) {
	int ret = 0;
	errno = 0;
	
	switch (info_requested) {
	case  SPE_COUNT_PHYSICAL_CPU_NODES:
		ret = _base_spe_count_physical_cpus(cpu_node);
		break;
	case SPE_COUNT_PHYSICAL_SPES:
		ret = _base_spe_count_physical_spes(cpu_node);
		break;
	case SPE_COUNT_USABLE_SPES:
		ret = _base_spe_count_usable_spes(cpu_node);
		break;
	default:
		errno = EINVAL;
		ret = -1;
	}
	return ret;
}
