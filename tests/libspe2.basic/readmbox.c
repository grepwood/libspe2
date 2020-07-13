/* --------------------------------------------------------------- */
/* (C) Copyright 2007  					           */
/* International Business Machines Corporation,			   */
/*								   */
/* All Rights Reserved.						   */
/* --------------------------------------------------------------- */

#include <stdio.h>
#include <spu_mfcio.h>

main()
{
	unsigned int data = 0;

	while (data != 8) {
		data = spu_read_in_mbox();
		printf("SPE received %d\n", data);
	}

	return 0;
}
