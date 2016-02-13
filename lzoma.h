#define AuthorID 0xA1Ef
#define AlgoID "LZOM"
#define Version 0x00

#define BLOCK_STORED 0x80000000
#define BLOCK_LAST 0x40000000

#define HISTORY_SIZE(dict_size) (32*1024<<dict_size)
#define BLOCK_SIZE(dict_size) (HISTORY_SIZE(dict_size) >> 4)

#define longlen 5400
#define hugelen 0x060000
#define breaklz 512
#define lzmagic 0x002FFe00*2
#define lzshift(top) ((9*top)>>3)

#define lzlow(total) ((total <= 400000) ? 60 :50)

#define breaklen 4
