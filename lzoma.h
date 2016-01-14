#define byte unsigned char
#define MAX_SIZE 16*1024*1024
#define NEXT_SIZE 1*1024*1024

#define longlen 5400
#define hugelen 0x060000
#define breaklz 512
#define lzmagic 0x02aFFe00*2
#define lzshift(top) ((9*top)>>3)
#define lzlow(total) ((total <= 49549) ? 80 : (total <= 652630) ? 60 :48)

#define breaklen 4
