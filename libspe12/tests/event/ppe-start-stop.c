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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libspe.h>


int main(int argc, char* argv[])
{
	spe_program_handle_t *binary,*binary1;
	speid_t spe_thread,spe_thread1;
	spe_gid_t spe_group;
	int max_spe, status;
	int numthread = 2;
	
	if (argc != 2) {
		printf("usage: pu spu-executable\n");
		exit(1);
	}
		
	binary = spe_open_image(argv[1]);
	if (!binary)
		exit(2);
	binary1 = spe_open_image(argv[1]);
	if (!binary1)
		exit(2);

	spe_group = spe_create_group(SCHED_OTHER, 0, 1);
	if (!spe_group) {
		printf("error: create_group.\n");
	}
	
	max_spe = spe_group_max(spe_group);
	printf("info: group_max=%i\n", max_spe);
	
        spe_thread  = spe_create_thread(spe_group, binary, NULL, NULL, 0, 0);
	usleep(500000);
        spe_thread1 = spe_create_thread(spe_group, binary, NULL, NULL, 0, 0);

	if (!spe_thread) {
		perror("Could not create thread");
		return -1;
	}


	while (1==1)
	{
		struct spe_event myevent;
		int ret;
		
		myevent.gid = spe_group;
		myevent.events = SPE_EVENT_STOP | SPE_EVENT_THREAD_EXIT ;
		
		ret = spe_get_event( &myevent,1, 100);
		
		if (!ret)
		{
			printf("get_event: timeout\n");
		}
		else
		{
			printf("get_event: revents=0x%04x\n",myevent.revents);
			printf("get_event: data=0x%04lx\n",myevent.data);
			printf("get_event: speid=%p\n",myevent.speid);

			spe_kill(myevent.speid,SIGCONT);

			if (myevent.data == 0x1199)
			{
				break;
			}
		}
		
	}
	usleep(200000);
	
	while (1==1)
	{
		struct spe_event myevent[2];
		int ret;
		
		myevent[0].gid = spe_group;
		myevent[0].events = SPE_EVENT_STOP | SPE_EVENT_THREAD_EXIT;
		myevent[1].gid = spe_group;
		myevent[1].events = SPE_EVENT_MAILBOX;
		
		ret = spe_get_event(&myevent[0], 2, 100);
		
		if (!ret)
		{
			printf("get_event: timeout\n");
		}
		else
		{
			printf("get_event: Got %i events.\n",ret);
			
			printf("get_event[0]: revents=0x%04x\n",myevent[0].revents);
			printf("get_event[0]: data=0x%04lx\n",myevent[0].data);
			printf("get_event[0]: speid=%p\n",myevent[0].speid);

			printf("get_event[1]: revents=0x%04x\n",myevent[1].revents);
			printf("get_event[1]: data=0x%04lx\n",myevent[1].data);
			printf("get_event[1]: speid=%p\n",myevent[1].speid);
			
			if (myevent[0].revents == SPE_EVENT_STOP)
				spe_kill(myevent[0].speid,SIGCONT);

			if (myevent[0].revents == SPE_EVENT_THREAD_EXIT)
			{
				spe_wait(myevent[0].speid, &status,0);
				numthread--;
				if (!numthread)
					break;
			}
		}
		
	}

	
	
	spe_close_image(binary);
	spe_close_image(binary1);
	
	return 0;
}
