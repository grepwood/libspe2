
#include <stdlib.h>
#include <pthread.h>
#include "libspe2.h"

struct thread_args {
	struct spe_context * ctx; 
	void * argp;
	void * envp;
};

static void * spe_thread(void * arg)
{
	int flags = 0;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	spe_program_handle_t * program;
	struct thread_args * arg_ptr;

	arg_ptr = (struct thread_args *) arg;

	program = spe_image_open("helloworld.spu.elf");
	spe_program_load(arg_ptr->ctx, program);
	spe_context_run(arg_ptr->ctx, &entry, flags, arg_ptr->argp, arg_ptr->envp, NULL);
	pthread_exit(NULL);
}

int main() {
	int thread_id;
	pthread_t pts;
	spe_context_ptr_t ctx;
	struct thread_args t_args;
	int value = 1;
        
	ctx = spe_context_create(0, NULL);
        
	t_args.ctx = ctx;
	t_args.argp = &value;

	thread_id = pthread_create( &pts, NULL, &spe_thread, &t_args);
              
	pthread_join (pts, NULL);
	spe_context_destroy (ctx);
	
	return 0;
}
 
