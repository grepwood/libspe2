/* --------------------------------------------------------------- */
/* (C) Copyright 2001,2006,                                        */
/* International Business Machines Corporation,                    */
/*                                                                 */
/* All Rights Reserved.                                            */
/* --------------------------------------------------------------- */
/* PROLOG END TAG zYx                                              */
#include<stdio.h>

typedef union {
        unsigned long long      ull;
        unsigned int            vp[2];
} addr64;


int main (int speid, addr64 argp, addr64 envp)
{
    printf("\t\tHello World! speid=0x%llx, argp=0x%08x%08x, envp=0x%08x%08x\n", speid,
           argp.vp[0],argp.vp[1], envp.vp[0],envp.vp[1]);

    return 0;
}
