#define AuthorID 0xA1Ef
#define AlgoID "LZOM"
#define Version 0x00

// TODO: these should become variables
#define HISTORY_SIZE 16*1024*1024
#define BLOCK_SIZE (HISTORY_SIZE >> 4)

#define longlen 5400
#define hugelen 0x060000
#define breaklz 512
#define lzmagic 0x02aFFe00*2
#define lzshift(top) ((9*top)>>3)

// TODO: this parameters should depend on block size
#define lzlow(total) ((total <= 49549) ? 80 : (total <= 652630) ? 60 :48)

#define breaklen 4
