#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "libspe2.h"

int main(void)
{
	spe_context_ptr_t ctx;
	int flags = 0;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	void * argp = NULL;
	void * envp = NULL;
	spe_program_handle_t * program;
	spe_stop_info_t stop_info;
	int rc;
	
	program = spe_image_open("helloworld.spu.elf");
	if (!program) {
		perror("spe_open_image");
		return -1;
	}
	
	ctx = spe_context_create(flags, NULL);
	if (ctx == NULL) {
		perror("spe_context_create");
		return -2;
	}
	if (spe_program_load(ctx, program)) {
		perror("spe_program_load");
		return -3;
	}
	rc = spe_context_run(ctx, &entry, 0, argp, envp, &stop_info);
	if (rc < 0)
		perror("spe_context_run");
	
	spe_context_destroy(ctx);

	return 0;
}

