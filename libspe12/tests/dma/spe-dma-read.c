#include <spu_intrinsics.h>

typedef union {
        unsigned long long      ull;
        unsigned int            vp[2];
} addr64;

volatile int result __attribute__ ((aligned (128)));

int main (int spuid, addr64 argp, addr64 envp)
{
        unsigned long long argAddress = argp.ull;

        /* Write to specified address */
        result = 23;

        __builtin_si_wrch((22), __builtin_si_from_uint(1));
        spu_mfcdma32((void *) &result, argAddress, 4, 0,
                     (((0) << 24) | ((0) << 16) | (0x40)));
        spu_mfcstat(0x2);

        /* Done */
        return result;
}

