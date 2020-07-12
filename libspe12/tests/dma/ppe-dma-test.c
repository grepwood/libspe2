#include <stdio.h>
#include <stdlib.h>

#include <libspe.h>

int main(int argc, char* argv[])
{
	spe_program_handle_t *binary;
	speid_t  threadnum;
	int status;

	static int test_result __attribute__ ((aligned (128)));
	
        /*  DMA - Write test. */
	
	binary = spe_open_image("spe-dma-write");
	if (!binary)
		exit(2);
	
	printf("Test result Addr: %p\n",&test_result);
        
        threadnum = spe_create_thread(0, binary, &test_result, NULL, 0, 0);
	spe_wait(threadnum,&status,0);

	printf("Thread returned status: %04x\n",status);
	printf("Test result: %04x\n",test_result);

	spe_close_image(binary);
	
        /*  DMA - Read test. */

	binary = spe_open_image("spe-dma-read");
	if (!binary)
		exit(2);
	
        threadnum = spe_create_thread(0, binary, &test_result, NULL, 0, 0);
	spe_wait(threadnum,&status,0);

	printf("Thread returned status: %04x\n",status);
	printf("Test result: %04x\n",test_result);
	spe_close_image(binary);
	
	return 0;
}
