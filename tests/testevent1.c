/**
 * Scenario 1: Portable Usage
 */
#include <libspe2.h>

#define MAX_EVENTS 8
#define SIZE 8
#define COUNT 1

int main()
{
	int i, rc, event_count;
	spe_event_handler_ptr_t evhandler;
	spe_event_unit_t event;
	spe_context_ptr_t ctx;
	spe_event_unit_t events[MAX_EVENTS];
	spe_stop_info_t stop_info;
	spe_program_handle_t * program;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	void * argp = NULL;
	void * envp = NULL;

	/* Create a context. */
	ctx = spe_context_create(SPE_EVENTS_ENABLE, NULL);
	if (ctx == NULL) {
		perror("spe_context_create");
		return -2;
	}

	/* load the program. */
	program = spe_image_open("hello");
	if (!program) {
		perror("spe_open_image");
		return -1;
	}

	if (spe_program_load(ctx, program)) {
		perror("spe_program_load");
		return -3;
	}
	
	/* Create a handle. */
	evhandler = spe_event_handler_create();

	/* Register events. */
	event.events = SPE_EVENT_SPE_STOPPED;
	event.spe = ctx;
	rc = spe_event_handler_register(evhandler, &event);

	/* run the context */
	rc = spe_context_run(ctx, &entry, 0, argp, envp, &stop_info);
	if (rc < 0)
		perror("spe_context_run");

	/* Get events. */
	event_count = spe_event_wait(evhandler, events, MAX_EVENTS, 0);
	printf("event_count: %d\n", event_count);

	/* Handle events. */
	for (i = 0; i < event_count; i++) {
		printf("event %d: %d\n", i, events[i].events);
		if (events[i].events & SPE_EVENT_SPE_STOPPED) {
			printf("received SPE_EVENT_SPE_STOPPED\n");
			rc = spe_stop_info_read(events[i].spe, &stop_info);
			printf("exit_code: %d\n", stop_info.result.spe_exit_code);
		}
	}

	/* Destroy the handle. */
	spe_event_handler_destroy(evhandler);

	/* Destroy the context. */
	spe_context_destroy(ctx);
  
	return 0;
}


