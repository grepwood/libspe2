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

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#include <libspe.h>


int main(int argc, char* argv[])
{
	spe_program_handle_t *binary;
	speid_t spe_thread;
	spe_gid_t spe_group;
	int max_spe, status;
	
	if (argc != 2) {
		printf("usage: pu spu-executable\n");
		exit(1);
	}
		
	binary = spe_open_image(argv[1]);
	if (!binary)
		exit(2);

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) {
		printf("error: create_group.\n");
	}
	
	max_spe = spe_group_max(spe_group);
	printf("info: group_max=%i\n", max_spe);
	
        spe_thread = spe_create_thread(spe_group, binary, NULL, NULL, 0, 0);

	if (spe_thread) {
		spe_wait(spe_thread, &status,0);
		printf("Thread returned status: %04x\n",status);
	} else
		perror("Could not create thread");
	
	spe_close_image(binary);
	
	return 0;
}
