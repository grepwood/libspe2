
#define SPE_LIBEA_CLASS 0x2105

#define SPE_LIBEA_UNUSED       0x00
#define SPE_LIBEA_CALLOC       0x01
#define SPE_LIBEA_FREE         0x02
#define SPE_LIBEA_MALLOC       0x03
#define SPE_LIBEA_REALLOC      0x04
#define SPE_LIBEA_POSIX_MEMALIGN   0x05
#define SPE_LIBEA_NR_OPCODES   0x06

extern int _base_spe_default_libea_handler(char *ls, unsigned long args);

#define SPE_LIBEA_OP_SHIFT  24
#define SPE_LIBEA_OP_MASK   0xff
#define SPE_LIBEA_DATA_MASK 0xffffff
#define SPE_LIBEA_OP(_v)   (((_v) >> SPE_LIBEA_OP_SHIFT) & SPE_LIBEA_OP_MASK)
#define SPE_LIBEA_DATA(_v) ((_v) & SPE_LIBEA_DATA_MASK)


