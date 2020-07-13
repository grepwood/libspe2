
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "libspe2.h"

#define MAX_EVENTS 8
#define SIZE 8
#define COUNT 1

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

	program = spe_image_open("hellointr.spu.elf");
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

int main() {
	int thread_id;
	int i, rc, event_count;
	pthread_t pts;
	spe_context_ptr_t ctx;
	struct thread_args t_args;
	int value = 1;
	int flags = SPE_EVENTS_ENABLE;
	spe_event_handler_ptr_t evhandler;
	spe_event_unit_t event;
	spe_event_unit_t events[MAX_EVENTS];
	unsigned int mbox_data[COUNT];
	spe_stop_info_t stop_info;
	int cont;
        
	if (!(ctx = spe_context_create(flags, NULL))) {
		perror("spe_create_context");
		return -2;
	}

	/* Create a handle. */
	evhandler = spe_event_handler_create();

	/* Register events. */
	event.events = SPE_EVENT_OUT_INTR_MBOX | SPE_EVENT_SPE_STOPPED;
	event.spe = ctx;
	rc = spe_event_handler_register(evhandler, &event);

	/* start pthread */
	t_args.ctx = ctx;
	t_args.argp = &value;

	thread_id = pthread_create( &pts, NULL, &spe_thread, &t_args);

 
	/* Get events. */
	cont = 1;
	while (cont) {
		event_count = spe_event_wait(evhandler, events, MAX_EVENTS, -1);
		printf("event_count %d\n", event_count);

		/* Handle events. */
		for (i = 0; i < event_count; i++) {
			printf("event %d: %d\n", i, events[i].events);
			if (events[i].events & SPE_EVENT_OUT_INTR_MBOX) {
				printf("SPE_EVENT_OUT_INTR_MBOX\n");
				rc = spe_out_intr_mbox_read(events[i].spe, 
							mbox_data, 
							COUNT, 
							SPE_MBOX_ANY_BLOCKING);
			}
			if (events[i].events & SPE_EVENT_SPE_STOPPED) {
				printf("SPE_EVENT_SPE_STOPPED\n");
				rc = spe_stop_info_read(events[i].spe, &stop_info);
				printf("stop_reason: %d\n", stop_info.stop_reason);
				cont = 0;
			}
		}
	}
		
	pthread_join (pts, NULL);

	/* Destroy the handle. */
	spe_event_handler_destroy(evhandler);

	/* Destroy the context. */
	spe_context_destroy (ctx);
	
	return 0;
}
 
