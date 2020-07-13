/* --------------------------------------------------------------- */
/* (C) Copyright 2007  					           */
/* International Business Machines Corporation,			   */
/*								   */
/* All Rights Reserved.						   */
/* --------------------------------------------------------------- */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "libspe2.h"

struct thread_args {
	struct spe_context * ctx; 
	void * argp;
	void * envp;
};

void * spe_thread(void * arg);

__attribute__((noreturn)) void * spe_thread(void * arg)
{
	int flags = 0;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	int rc;
	spe_program_handle_t * program;
	struct thread_args * arg_ptr;

	arg_ptr = (struct thread_args *) arg;

	program = spe_image_open("readmbox.spu.elf");
	if (!program) {
		perror("spe_image_open");
		pthread_exit(NULL);
	}

	if (spe_program_load(arg_ptr->ctx, program)) {
		perror("spe_program_load");
		pthread_exit(NULL);
	}

	rc = spe_context_run(arg_ptr->ctx, &entry, flags, arg_ptr->argp, arg_ptr->envp, NULL);
	if (rc < 0)
		perror("spe_context_run");

	pthread_exit(NULL);
}

int main(void) 
{
	unsigned int i;
	unsigned int msgs[8];
	struct thread_args t_args;
	int thread_id;
	pthread_t pts;
	spe_context_ptr_t ctx;
	int rc;

	ctx = spe_context_create(0, NULL);
	if (ctx == NULL) {
		perror("spe_context_create");
		return -2;
	}

	// check initial status
	printf("in_mbox:\tspace for %d messages\n", spe_in_mbox_status(ctx));
	printf("out_mbox:\tcontains %d messages\n", spe_out_mbox_status(ctx));
	printf("out_intr_mbox:\tcontains %d messages\n", spe_out_intr_mbox_status(ctx));

	// fill in_mbox one by one
	for (i=0; i<4; i++) {
		rc = spe_in_mbox_write(ctx, &i, 1, SPE_MBOX_ALL_BLOCKING);
		printf("writing %d to in_mbox rc = %d\n", i, rc); 
		printf("in_mbox:\tspace for %d messages\n", spe_in_mbox_status(ctx));
	}
	// try to write one more, but nonblocking
	rc = spe_in_mbox_write(ctx, &i, 1, SPE_MBOX_ANY_NONBLOCKING);
	printf("writing %d to in_mbox rc = %d\n", i, rc);

	t_args.ctx = ctx;

	// now empty the in_mox from the spe side 
	thread_id = pthread_create( &pts, NULL, &spe_thread, &t_args);

	// write several messages at once
	for (i=0; i<8; i++) {
		msgs[i] = i+1;
	}
	
	rc = spe_in_mbox_write(ctx, msgs, 8, SPE_MBOX_ALL_BLOCKING);
	printf("writing 8 messages to in_mbox rc = %d\n", rc); 
	printf("in_mbox:\tspace for %d messages\n", spe_in_mbox_status(ctx));
	printf("in_mbox:\tspace for %d messages\n", spe_in_mbox_status(ctx));
	printf("in_mbox:\tspace for %d messages\n", spe_in_mbox_status(ctx));

	pthread_join (pts, NULL);
	
	spe_context_destroy(ctx);


	return 0;

}

