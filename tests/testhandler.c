/* --------------------------------------------------------------- */
/* (C) Copyright 2007  					           */
/* International Business Machines Corporation,			   */
/*								   */
/* All Rights Reserved.						   */
/* --------------------------------------------------------------- */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <libspe2.h>


int local_handler(void *bla, unsigned int npc)
{
	return 0;
}

int main(void)
{
	int i, rc; 
	void *  handler;

	for (i = -1; i < 257; i++) {
		handler = spe_callback_handler_query(i);
		printf("\nquery\t\t%d (%d)\n", i, errno);

		rc = spe_callback_handler_register(&local_handler, i, SPE_CALLBACK_NEW);
		printf("register\t%d %d (%d)\n", i, rc, errno);
		handler = spe_callback_handler_query(i);
		printf("\nquery\t\t%d (%d)\n", i, errno);

		rc = spe_callback_handler_deregister(i);
		printf("deregister\t%d %d (%d)\n", i, rc, errno);
		handler = spe_callback_handler_query(i);
		printf("\nquery\t\t%d (%d)\n", i, errno);

		rc = spe_callback_handler_register(&local_handler, i, SPE_CALLBACK_UPDATE);
		printf("update\t\t%d %d (%d)\n", i, rc, errno);
		handler = spe_callback_handler_query(i);
		printf("\nquery\t\t%d (%d)\n", i, errno);
	}

	return 0;
}
