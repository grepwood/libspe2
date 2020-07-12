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

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>

#include <libspe.h>

static int basic_test(void)
{
	spe_program_handle_t *binary[16];
	speid_t spe_thread[256];
	spe_gid_t spe_group;
	spe_gid_t spe_groups[16];
	int result = 0;
	int i, j, a, max_spe, threads, status, ret;

	static int test_result __attribute__ ((aligned(128)));

	/* Basic Test */

	printf("\nStep 1: spe_create_group() ");
	
	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) 
	{
		printf("error: create_group.\n");
		result++;
	}

	printf("\nStep 1a: spe_destroy_group() ");
	
	if (spe_destroy_group(spe_group))
	{
		printf("error: destroy_group.\n");
	}
	
	if (spe_destroy_group(spe_group))
	{
		printf("Group gone.");
	}
	
	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) 
	{
		printf("error: create_group.\n");
		result++;
	}
	
	printf(" OK.\n");
	printf("  spe_group=%p.\n",spe_group);
 
	printf("\nStep 2: spe_group_max() returns:");
	
	max_spe = spe_group_max(spe_group);
	printf("%i\n", max_spe);

	printf("\nStep 3: spe_get_threads(spe_gid_t,NULL) returns:");
	threads = spe_get_threads(spe_group,NULL);
	printf("%i ", threads);
	if(threads)
	{
		printf(" ERROR.\n");
		result++;
	}
	else
		printf(" OK.\n");
	
	printf("\nStep 4a: spe_open_image(\"spe-test-pause\") ");
	binary[0] = spe_open_image("spe-test-pause");
	if (!binary[0])
	{
		printf(" ERROR.\n");
		exit(2);
	}
	printf(" OK.\n");


	printf("\nStep 4b: spe_open_image(\"spe-test-pause\") (again) ");
	binary[1] = spe_open_image("spe-test-pause");
	if (!binary[1])
	{
		printf(" ERROR.\n");
		exit(2);
	}
	printf(" OK.\n");


	printf("\nStep 5a: spe_create_thread() ");
	spe_thread[0] = spe_create_thread(spe_group, binary[0], &test_result, NULL, 0xffff, 0);
	if (!spe_thread[0])
	{
		printf(" ERROR.\n");
		exit(2);
	}
	else
	{
		printf("OK.\n");
		printf("  Returned speid=%p\n",spe_thread[0]);
	}
	
	usleep(500000);

	printf("\nStep 5b: spe_create_thread() (again) ");
	spe_thread[1] = spe_create_thread(spe_group, binary[1], &test_result, NULL, 0xffff, 0);
	if (!spe_thread[1])
	{
		printf(" ERROR.\n");
		exit(2);
	}
	else
	{
		printf("OK.\n");
		printf("  Returned speid=%p\n",spe_thread[1]);
	}
	
	printf("\nStep 5c: spe_destroy_group() (delayed)");
	if (spe_destroy_group(spe_group))
	{
		printf("error: destroy_group.\n");
	}
	

	printf("\nStep 6: spe_get_threads(spe_gid_t,NULL) returns:");
	threads = spe_get_threads(spe_group,NULL);
	printf("%i ", threads);
	if (threads != 2)
	{
		printf(" ERROR.\n");
		result++;
	}
	else
	{
		speid_t speids[16];
		int i;
		
		printf(" OK.\n");
		
		threads=spe_get_threads(spe_group,speids);
		for (i=0 ; i<threads ; i++)
		{
			printf("  Thread[%i] = %p\n",i,speids[i]);
		}
		
	}

	printf("\nStep 7a: spe_wait() ");
	ret = spe_wait(spe_thread[0], &status, 0);
	if(!ret && status== 0)
	{
		speid_t speids[16];
		int i;
		
		printf("OK.\n");
		printf("  Thread returned status=%i\n",status);
		printf("  Remaining threads:\n");
		
		threads=spe_get_threads(spe_group,speids);
		for (i=0 ; i<threads ; i++)
		{
			printf("  Thread[%i] = %p\n",i,speids[i]);
		}

	}
	else
	{
		printf("ERROR.\n");
		result++;
	
	}

	printf("\nStep 7b: spe_wait() ");
	ret = spe_wait(spe_thread[1], &status, 0);
	if(!ret && status== 0)
	{
		speid_t speids[16];
		int i;
		
		printf("OK.\n");
		printf("  Thread returned status=%i\n",status);
		printf("  Remaining threads:\n");
		
		threads=spe_get_threads(spe_group,speids);
		for (i=0 ; i<threads ; i++)
		{
			printf("  Thread[%i] = %p\n",i,speids[i]);
		}

	}
	else
	{
		printf("ERROR.\n");
		result++;
	
	}
	
	printf("\nStep 8: spe_get_threads(spe_gid_t,NULL) returns:");
	threads = spe_get_threads(spe_group,NULL);
	printf("%i ", threads);
	if(!threads)
	{
		printf(" ERROR.\n");
		result++;
	}
	else
		printf(" OK.\n");

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) 
	{
		printf("error: create_group.\n");
		result++;
	}

	printf("\nClosing images.\n");
	ret = spe_close_image(binary[0]);
	if (!ret)
	{
		printf("Image 1 - OK. (%i)\n",ret);
	}
	else
	{
		printf("Image could not be unmapped - errno:%i\n",errno);
		perror("Reason");
	}
	
	ret = spe_close_image(binary[1]);
	if (!ret)
	{
		printf("Image 2 - OK. (%i)\n",ret);
	}
	else
	{
		printf("Image could not be unmapped - errno:%i\n",errno);
		perror("Reason");
	}
	

	printf("\nStep 9: Load tests (these may take some time)\n");

	printf("  a.) lots of threads seqentially.");
	
	binary[0] = spe_open_image("spe-test-start-stop");

	for (i=0; i< 1024; i++)
	{
		spe_thread[0] = spe_create_thread(spe_group, binary[0], 
						  &test_result, NULL, 0xffff, 0);

		if (!spe_thread[0])
               	{
			printf("\n    failed to create load thread %i.",i);
			result++;
			break;
		}
		spe_wait(spe_thread[0], &status, 0);
	}
	if (i==1024)
		printf(" OK.\n");

	printf("  b.) lots of threads seqentially and parallel in groups.");
	
	binary[0] = spe_open_image("spe-test-start-stop");

	for (i=0; i< 1024; i++)
	{
		for (j=0; j< 8; j++)
		{
			spe_thread[j] = spe_create_thread(spe_group, binary[0], 
						  &test_result, NULL, 0xffff, 0);

			if (!spe_thread[j])
               		{
				printf("\n    failed to create load thread %i,%i.",i,j);
				result++;
				break;
			}
		}
		for (j=0; j< 8; j++)
		{
			spe_wait(spe_thread[j], &status, 0);
		}
	}
	if (i==1024)
		printf(" OK.\n");

	printf("\nClosing images.\n");
	ret = spe_close_image(binary[0]);
	if (!ret)
	{
		printf("Image 1 - OK. (%i)\n",ret);
	}
	else
	{
		printf("Image could not be unmapped - errno:%i\n",errno);
		perror("Reason");
	}
	
        printf("\nStep 10: spe_kill(SIGKILL)\n");

        binary[0] = spe_open_image("spe-test-block");

        printf("\nStep 10a: spe_create_thread() ");
        spe_thread[0] = spe_create_thread(spe_group, binary[0], &test_result, NULL, 0xffff, 0);
        if (!spe_thread[0])
        {
                printf(" ERROR.\n");
                exit(2);
        }
        else
        {
                printf("OK.\n");
                printf("  Returned speid=%p\n",spe_thread[0]);
        }

        usleep(500000);

        printf("\nStep 10b: spe_kill() ");
        spe_kill(spe_thread[0],SIGKILL);

        printf("\nStep 10c: spe_wait() ");
        ret = spe_wait(spe_thread[0], &status, 0);

	printf("\nClosing images.\n");
	ret = spe_close_image(binary[0]);
	if (!ret)
	{
		printf("Image 1 - OK. (%i)\n",ret);
	}
	else
	{
		printf("Image could not be unmapped - errno:%i\n",errno);
		perror("Reason");
	}
	
        printf("\nStep 11: spe_map_ps_area()\n");
	
        binary[0] = spe_open_image("spe-test-block");

        printf("\nStep 11a: spe_create_thread() ");
        spe_thread[0] = spe_create_thread(spe_group, binary[0], &test_result, NULL, 0xffff, SPE_MAP_PS);
        if (!spe_thread[0])
        {
                printf(" ERROR.\n");
                exit(2);
        }
        else
        {	
		void* map;
		
                printf("OK.\n");
                printf("  Returned speid=%p\n",spe_thread[0]);
		
		printf("\nStep 11b: spe_get_ps_area(speid, SPE_MSSYNC_AREA) ");
		
		map=spe_get_ps_area(spe_thread[0],SPE_MSSYNC_AREA);

		if ( map != MAP_FAILED )
		{
			printf("OK.\n");
			printf("  Returned map=%p\n",map);
		}
		else
		{
			printf("FAILED.\n");
		}
		
		printf("\nStep 11c: spe_get_ps_area(speid, SPE_MFC_COMMAND_AREA) ");
		
		map=spe_get_ps_area(spe_thread[0],SPE_MFC_COMMAND_AREA);

		if ( map != MAP_FAILED )
		{
			printf("OK.\n");
			printf("  Returned map=%p\n",map);
		}
		else
		{
			printf("FAILED.\n");
		}
		
		printf("\nStep 11d: spe_get_ps_area(speid, SPE_CONTROL_AREA) ");
		
		map=spe_get_ps_area(spe_thread[0],SPE_CONTROL_AREA);

		if ( map != MAP_FAILED )
		{
			printf("OK.\n");
			printf("  Returned map=%p\n",map);
		}
		else
		{
			printf("FAILED.\n");
		}
		
		printf("\nStep 11e: spe_get_ps_area(speid, SPE_SIG_NOTIFY_1_AREA) ");
		
		map=spe_get_ps_area(spe_thread[0],SPE_SIG_NOTIFY_1_AREA);

		if ( map != MAP_FAILED )
		{
			printf("OK.\n");
			printf("  Returned map=%p\n",map);
		}
		else
		{
			printf("FAILED.\n");
		}
	
		printf("\nStep 11f: spe_get_ps_area(speid, SPE_SIG_NOTIFY_2_AREA) ");
		
		map=spe_get_ps_area(spe_thread[0],SPE_SIG_NOTIFY_2_AREA);

		if ( map != MAP_FAILED )
		{
			printf("OK.\n");
			printf("  Returned map=%p\n",map);
		}
		else
		{
			printf("FAILED.\n");
		}
        }

        printf("\nStep 11g: spe_kill() ");
        spe_kill(spe_thread[0],SIGKILL);

        printf("\nStep 11h: spe_wait() ");
        ret = spe_wait(spe_thread[0], &status, 0);
	
	printf("\nClosing images.\n");
	ret = spe_close_image(binary[0]);
	if (!ret)
	{
		printf("Image 1 - OK. (%i)\n",ret);
	}
	else
	{
		printf("Image could not be unmapped - errno:%i\n",errno);
		perror("Reason");
	}
/*      
 *	This test is disabled because there is no scheduler support in this release.
 *      
	printf("\nStep 12: Composite tests\n");

	printf("  a.) Starting 64 Threads in parallel.\n");
	
	binary[0] = spe_open_image("spe-test-mbox-block");

	for (a=0; a < 4; a++)
	{
		spe_groups[a] = spe_create_group(SCHED_OTHER, 0, 1); 
		for (i=0; i< 16; i++)
		{
			spe_thread[a*16 + i] = spe_create_thread(spe_groups[a], binary[0], 
							  &test_result, NULL, 0xffff, 0);
	
			if (!spe_thread[a*16 + i])
       	        	{
				printf("\n    failed to create thread %i.",i);
				result++;
				exit(2);
			}
			//spe_wait(spe_thread[0], &status, 0);
		}
		printf("%i.\n",(a*16+16));
	}
	
	printf("  b.) Waiting for 64 mailbox messages. ( one by one)\n");

	for (i=0; i< 64; i++)
	{
                struct spe_event myevent;
		int ret = 0;

                myevent.gid = spe_groups[i/16];
                myevent.events = SPE_EVENT_MAILBOX;


                while(!ret)
                {
                	ret = spe_get_event( &myevent,1, 100);
                        if (!ret)
				printf("get_event: timeout\n");
                }

                if (myevent.data != 0xaabb)
                {
                        printf("mailbox data error !!\n");
			result++;
                }
	}
	
	printf("  c.) Waiting for 64 stop messages. ( with 8 event members )\n");
	
	for (i=0; i< 8; i++)
	{
                struct spe_event myevent[8];
		int ret = 0;
		int allret = 0;

                myevent[0].gid = spe_groups[i/2];
                myevent[0].events = SPE_EVENT_STOP;
                myevent[1].gid = spe_groups[i/2];
                myevent[1].events = SPE_EVENT_STOP;
                myevent[2].gid = spe_groups[i/2];
                myevent[2].events = SPE_EVENT_STOP;
                myevent[3].gid = spe_groups[i/2];
                myevent[3].events = SPE_EVENT_STOP;
                myevent[4].gid = spe_groups[i/2];
                myevent[4].events = SPE_EVENT_STOP;
                myevent[5].gid = spe_groups[i/2];
                myevent[5].events = SPE_EVENT_STOP;
                myevent[6].gid = spe_groups[i/2];
                myevent[6].events = SPE_EVENT_STOP;
                myevent[7].gid = spe_groups[i/2];
                myevent[7].events = SPE_EVENT_STOP;

                while( allret < 8 )
                {
                	ret = spe_get_event(myevent,8, 100);
                        if (!ret)
			{
				printf("get_event: timeout\n");
			}
			else
			{
				for (j=0; j < 8; j++)
				{
					if(myevent[j].revents && SPE_EVENT_STOP)
					{
                				if (myevent[j].data == 0x1188)
						{
						   	spe_kill(myevent[j].speid,SIGCONT);
							allret++;
						}
					}
				}
			}
                }
	}
	
	printf("  d.) Waiting for 64 mailbox messages. ( with 16 event members )\n");
	
	for (i=0; i< 4; i++)
	{
                struct spe_event myevent[16];
		int ret = 0;
		int allret = 0;
		int tcount = 0;

                myevent[0].gid = spe_groups[i];
                myevent[0].events = SPE_EVENT_MAILBOX;
                myevent[1].gid = spe_groups[i];
                myevent[1].events = SPE_EVENT_MAILBOX;
                myevent[2].gid = spe_groups[i];
                myevent[2].events = SPE_EVENT_MAILBOX;
                myevent[3].gid = spe_groups[i];
                myevent[3].events = SPE_EVENT_MAILBOX;
                myevent[4].gid = spe_groups[i];
                myevent[4].events = SPE_EVENT_MAILBOX;
                myevent[5].gid = spe_groups[i];
                myevent[5].events = SPE_EVENT_MAILBOX;
                myevent[6].gid = spe_groups[i];
                myevent[6].events = SPE_EVENT_MAILBOX;
                myevent[7].gid = spe_groups[i];
                myevent[7].events = SPE_EVENT_MAILBOX;

                myevent[8].gid = spe_groups[i];
                myevent[8].events = SPE_EVENT_MAILBOX;
                myevent[9].gid = spe_groups[i];
                myevent[9].events = SPE_EVENT_MAILBOX;
                myevent[10].gid = spe_groups[i];
                myevent[10].events = SPE_EVENT_MAILBOX;
                myevent[11].gid = spe_groups[i];
                myevent[11].events = SPE_EVENT_MAILBOX;
                myevent[12].gid = spe_groups[i];
                myevent[12].events = SPE_EVENT_MAILBOX;
                myevent[13].gid = spe_groups[i];
                myevent[13].events = SPE_EVENT_MAILBOX;
                myevent[14].gid = spe_groups[i];
                myevent[14].events = SPE_EVENT_MAILBOX;
                myevent[15].gid = spe_groups[i];
                myevent[15].events = SPE_EVENT_MAILBOX;
                
		while( allret < 16 )
                {
                	
			ret = spe_get_event(myevent, 16, 100);
                        if (!ret)
			{
				printf("get_event: timeout\n");
				tcount++;
				if (tcount==3)
				{
					result++;
					break;
				}
			}
			else
			{
				printf("got:%i \n",ret);
				
				for (j=0; j < 16; j++)
				{
					if(myevent[j].revents && SPE_EVENT_MAILBOX)
					{
                				if (myevent[j].data != 0xccdd)
                				{
                     			   		printf("mailbox data error !!\n");
							result++;
                				}
						allret++;
					}
				}
			}
                }
	}
	
	printf("  e.) Waiting for 64 stop messages. ( one by one )\n");
	
	for (i=0; i< 64; i++)
	{
                struct spe_event myevent;
		int tcount = 0;

                myevent.gid = spe_groups[i/16];
                myevent.events = SPE_EVENT_STOP;

               	ret = spe_get_event(&myevent,1, 100);
                if (!ret)
		{
			printf("get_event: timeout\n");
			tcount++;
			if (tcount==3)
			{
				result++;
				break;
			}
		}
		else
		{
                	if (myevent.data != 0x1189)
               		{
               	   		printf("si_stop data error !! (got: 0x%04x)\n",(int)myevent.data);
				result++;
               		}
		}
	}
	
	printf("  f.) Resuming 64 SPEs one by one and waiting for them to end.\n");
	
	for (i=0; i< 64; i++)
	{
		spe_kill(spe_thread[i],SIGCONT);

		ret = spe_wait(spe_thread[i], &status, 0);
	}

	printf("\nClosing images.\n");
	ret = spe_close_image(binary[0]);

	*/
	
	printf("\n\n\nTEST RESULT:.\n");
	printf("-------------\n");
	if (!result)
	{
		printf("All tests completed ok.\n");
	}
	else
	{
		printf("Errors were encountered - check your installation.\n");
	}

	return 0;
}

static int ppe_mbox_test(void)
{
	spe_program_handle_t *binary;
	speid_t spe_thread;
	spe_gid_t spe_group;
	const uint32_t mbox_magic = 0xDEAD4EAD;
	uint32_t mbox_data;
	int status;
	int ret;
	static int test_result __attribute__ ((aligned(128)));

	binary = spe_open_image("spe-test-mboxwait");
	if (!binary) {
		perror("spe_open_image");
		return 1;
	}

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) {
		perror("spe_create_group");
		return 2;
	}

	spe_thread = spe_create_thread(spe_group, binary,
					 &test_result, NULL, 0xffff, 0);
	if (!spe_thread) {
		perror("spe_create_thread");
		return 3;
	}

	ret = spe_write_in_mbox (spe_thread, mbox_magic);
	if (ret) {
		perror("spe_write_in_mbox");
		return 8;
	}

	/* something magic happens here */
	
	do {
		mbox_data = spe_stat_out_mbox(spe_thread);
		usleep(250000);		
	} while (mbox_data != 1);
	mbox_data = spe_read_out_mbox(spe_thread);
	
	if (mbox_data != mbox_magic) {
		perror("spe_read_out_mbox");
		return 7;
	}

	ret = spe_wait(spe_thread, &status, 0);
	if (ret) {
		perror("spe_wait");
		return 8;
	}

	return ret;
}

static int ppe_test_read_ibox(spe_gid_t g, speid_t s, uint32_t *data)
{
	int ret;
	struct spe_event e = {
		.gid = g,
		.events = SPE_EVENT_MAILBOX,
	};
	struct timeval tv1, tv2;
	const int timeout = 10000; /* milliseconds */

	ret = gettimeofday(&tv1, 0);
	if (ret) {
		perror("gettimeofday");
		return 1;
	}

	ret = spe_get_event(&e, 1, timeout);
	if (!ret) {
		fprintf(stderr, "spe_get_event: timeout\n");
		return 2;
	}

	ret = gettimeofday(&tv2, 0);
	if (ret) {
		perror("gettimeofday");
		return 3;
	}

	if ((((tv2.tv_sec - tv1.tv_sec) * 1000) +
	     ((tv2.tv_usec - tv1.tv_usec) / 1000))
	    >= timeout) {
		fprintf(stderr, "spe_get_event: probably timed out\n");
		return 4;
	}

	*data = e.data;

	if (e.revents != SPE_EVENT_MAILBOX) {
		fprintf(stderr, "spe_get_event: unexpected event %d\n",
				e.revents);
		return 5;
	}

	if (e.speid != s) {
		fprintf(stderr, "spe_get_event: unexpected speid %p\n",
				e.speid);
		return 6;
	}
	return 0;
}

static int ppe_ibox_test(void)
{
	spe_program_handle_t *binary;
	speid_t spe_thread;
	spe_gid_t spe_group;
	const uint32_t mbox_magic = 0xDEAD4EAD;
	uint32_t mbox_data;
	int status;
	int ret;
	static int test_result __attribute__ ((aligned(128)));

	binary = spe_open_image("spe-test-ppedma");
	if (!binary) {
		perror("spe_open_image");
		return 1;
	}

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) {
		perror("spe_create_group");
		return 2;
	}

	spe_thread = spe_create_thread(spe_group, binary,
					 &test_result, NULL, 0xffff, 0);
	if (!spe_thread) {
		perror("spe_create_thread");
		return 3;
	}

	ret = ppe_test_read_ibox(spe_group, spe_thread, &mbox_data);
	if (ret)
		return 5;

	ret = spe_write_in_mbox (spe_thread, mbox_magic);
	if (ret) {
		perror("spe_write_in_mbox");
		return 6;
	}

	ret = spe_wait(spe_thread, &status, 0);
	if (ret) {
		perror("spe_wait");
		return 7;
	}

	return 0;
}

#if 0
static int ppe_ibox2_test(void)
{
	/* one bug in spufs caused get_event to succeed only once
	 * on the ibox file, so we try it twice here */
	spe_program_handle_t *binary;
	speid_t spe_thread;
	spe_gid_t spe_group;
	const uint32_t mbox_magic = 0xDEAD4EAD;
	uint32_t mbox_data;
	int status;
	int ret;
	static int test_result __attribute__ ((aligned(128)));

	binary = spe_open_image("spe-test-ibox2");
	if (!binary) {
		perror("spe_open_image");
		return 1;
	}

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) {
		perror("spe_create_group");
		return 2;
	}

	spe_thread = spe_create_thread(spe_group, binary,
					 &test_result, NULL, 0xffff, 0);
	if (!spe_thread) {
		perror("spe_create_thread");
		return 3;
	}

	ret = ppe_test_read_ibox(spe_group, spe_thread, &mbox_data);
	if (ret)
		return 5;

	ret = spe_write_in_mbox (spe_thread, mbox_magic);
	if (ret) {
		perror("spe_write_in_mbox1");
		return 6;
	}

	ret = ppe_test_read_ibox(spe_group, spe_thread, &mbox_data);
	if (ret)
		return 7;

	ret = spe_write_in_mbox (spe_thread, mbox_magic);
	if (ret) {
		perror("spe_write_in_mbox2");
		return 8;
	}

	ret = spe_wait(spe_thread, &status, 0);
	if (ret) {
		perror("spe_wait");
		return 9;
	}

	return 0;
}
#endif

static int ppe_dma_test(int (*fn)(speid_t, spe_gid_t, uint32_t, void*))
{
	spe_program_handle_t *binary;
	speid_t spe_thread;
	spe_gid_t spe_group;
	const uint32_t mbox_magic = 0xDEAD4EAD;
	uint32_t ls_offs;
	void *ls_buf;
	int status;
	int ret;
	static int test_result __attribute__ ((aligned(128)));

	binary = spe_open_image("spe-test-ppedma");
	if (!binary) {
		perror("spe_open_image");
		return 1;
	}

	spe_group = spe_create_group(SCHED_OTHER, 0, 0);
	if (!spe_group) {
		perror("spe_create_group");
		return 2;
	}

	spe_thread = spe_create_thread(spe_group, binary,
					 &test_result, NULL, 0xffff, 0);
	if (!spe_thread) {
		perror("spe_create_thread");
		return 3;
	}

	ls_buf = spe_get_ls(spe_thread);
	if (!ls_buf) {
		perror("spe_get_ls");
		return 4;
	}

	ret = ppe_test_read_ibox(spe_group, spe_thread, &ls_offs);
	if (ret)
		return 5;

	ls_buf += ls_offs;

	ret = fn(spe_thread, spe_group, ls_offs, ls_buf);
	if (ret)
		return ret;

	ret = spe_write_in_mbox (spe_thread, mbox_magic);
	if (ret) {
		perror("spe_write_in_mbox");
		return 9;
	}

	ret = spe_wait(spe_thread, &status, 0);
	if (ret) {
		perror("spe_wait");
		return 10;
	}

	return 0;
}

static int do_ppe_dma_put(speid_t t, spe_gid_t g,
				 uint32_t ls_offs, void *ls_buf)
{
	static const int size = 16 * 1024;
	static int buf[size / sizeof(int)] __attribute__((aligned(128)));
	int ret;
	struct spe_event e = {
		.gid = g,
		.events = SPE_EVENT_TAG_GROUP,
	};

	memset(ls_buf, 0, size);
	memset(buf, 0x5a, size);

	ret = spe_mfc_put(t, ls_offs, buf, size, 1, 0, 0);
	if (ret) {
		perror("do_ppe_dma_put: spe_mfc_put");
		return 9;
	}

	ret = spe_get_event(&e, 1, 1000);
	if (!ret) {
		fprintf(stderr, "do_ppe_dma_put: spe_get_event: timeout\n");
//		return 7;
	}

	if (memcmp(buf, ls_buf, size) != 0) {
		fprintf(stderr, "do_ppe_dma_put: miscompare\n");
		return 10;
	}

	return 0;
}

static int ppe_dma_put(void)
{
	return ppe_dma_test(do_ppe_dma_put);
}

static int do_ppe_dma_get(speid_t t, spe_gid_t g,
				 uint32_t ls_offs, void *ls_buf)
{
	static const int size = 16 * 1024;
	static int buf[size / sizeof(int)] __attribute__((aligned(128)));
	int ret;
	struct spe_event e = {
		.gid = g,
		.events = SPE_EVENT_TAG_GROUP,
	};

	memset(ls_buf, 0, size);
	memset(buf, 0x5a, size);

	ret = spe_mfc_get(t, ls_offs, buf, size, 1, 0, 0);
	if (ret) {
		perror("do_ppe_dma_get: spe_mfc_get");
		return 9;
	}

	ret = spe_get_event(&e, 1, 1000);
	if (!ret) {
		fprintf(stderr, "do_ppe_dma_get: spe_get_event: timeout\n");
//		return 7;
	}

	if (memcmp(buf, ls_buf, size) != 0) {
		fprintf(stderr, "do_ppe_dma_get: miscompare\n");
		return 10;
	}

	return 0;
}

static int ppe_dma_get(void)
{
	return ppe_dma_test(do_ppe_dma_get);
}

static int do_ppe_dma_mput(speid_t t, spe_gid_t g,
				 uint32_t ls_offs, void *ls_buf)
{
	static const int size = 64 * 1024;
	static const int fragment = 512;
	static int buf[size / sizeof(int) * 4] __attribute__((aligned(128)));
	int ret;
	struct spe_event e = {
		.gid = g,
		.events = SPE_EVENT_TAG_GROUP,
	};
	int i;

	memset(ls_buf, 0xaa, size);
//	memset(buf, 0x5a, size);

	for (i = 0; i < size/(fragment); i++) {
		ret = spe_mfc_put(t, ls_offs + fragment*i,
				(void*) buf + fragment *i,
				  fragment, 1, 0, 0);
		if (ret) {
			perror("do_ppe_dma_mput: spe_mfc_get");
			fprintf(stderr, "count %d\n", i);
			return 9;
		}
	}
	ret = spe_get_event(&e, 1, 1000);
	if (!ret) {
		fprintf(stderr, "do_ppe_dma_mput: spe_get_event: timeout\n");
//		return 7;
	}

	if (memcmp(buf, ls_buf, size) != 0) {
		fprintf(stderr, "do_ppe_dma_mput: miscompare\n");
		return 10;
	}

	return 0;
}

static int ppe_dma_mput(void)
{
	return ppe_dma_test(do_ppe_dma_mput);
}

static struct {
	int (*fn)(void);
	const char *name;
} tests[] = {
	{ basic_test, "basic", },
	{ ppe_mbox_test, "mbox", },
	{ ppe_ibox_test, "ibox", },
/*	{ ppe_ibox2_test, "ibox2", }, */
	{ ppe_dma_get, "ppeget", },
	{ ppe_dma_put, "ppeput", },
	{ ppe_dma_mput, "ppemput", },
};

static const int num_tests = sizeof (tests) / sizeof (tests[0]);

int runtest(char *name)
{
	int i, ret;

	ret = 0;
	for (i = 0; i < num_tests; i++) {
		if (strcmp(tests[i].name, name) == 0)	{
			fprintf(stderr, "%02d: %s\n", i, tests[i].name);
			ret = tests[i].fn();
			fprintf(stderr, "%02d: return: %d\n", i, ret);
			break;
		}
	}
	return ret;

}

int runalltests(void)
{
	int i, ret;

	for (i = 0; i < num_tests; i++) {
		fprintf(stderr, "%02d: %s\n", i, tests[i].name);
		ret = tests[i].fn();
		fprintf(stderr, "%02d: return: %d\n", i, ret);
		if (ret)
			break;
	}
	return ret;
}

int main(int argc, char *argv[])
{
	int i;
	int ret;

	ret = 0;
	if (argc == 1)
		ret = runalltests();
	else for (i = 1; i < argc; i++) {
		ret = runtest(argv[i]);
		if (ret)
			break;
	}

	return ret;
}
