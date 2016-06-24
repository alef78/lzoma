// test file decompression using LZOMA algoritm
// (c) Alexandr Efimov, 2015-2016
// License: GPL v2 or later

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <stdint.h>
#include <string.h>
//#include <x86intrin.h>

#ifndef O_BINARY
  #ifdef _O_BINARY
    #define O_BINARY _O_BINARY
  #else
    #define O_BINARY 0
  #endif
#endif

#include "lzoma.h"

uint8_t *in_buf; /* text to be decoded */
uint8_t *out_buf;/* decoded text + history */

//#define getbit (((bits=bits&0x7f? bits+bits :  (((unsigned)(*src++))<<1)+1)>>8)&1)
#define getbit ((bits=bits&0x7fffffff? (resbits=bits,bits+bits) :  (src+=4,resbits=*((uint32_t *)(src-4)),(resbits<<1)+1)),resbits>>31)

#define getcode(bits, src, ptotal) {\
  int total = (ptotal);\
  ofs=0;\
  long int res=0;\
  int x=256;\
  int top=0;\
  top=lzlow(total);\
  res=*src++;\
\
  while (1) {\
    x+=x;\
    if (x>=total+top) break;\
    if (x & lzmagic)\
      top=lzshift(top);\
    if (res<top) {  goto getcode_doneit;}\
    ofs-=top;\
    total+=top;\
    top+=top;\
    res+=res+getbit;\
  }\
  x-=total;\
  if (res>=x) { \
    res+=res+getbit;\
    res-=x;\
  }\
getcode_doneit: \
  ofs+=res;\
}

#define getlen(bits, src) {\
  long int res=0;\
  \
  if (getbit==0) {\
    len+=getbit;\
    goto getlen_0bit;\
  }\
  len+=2;\
  while (1) {  \
    res+=res+getbit;\
    if (getbit==0) break;\
    res++;\
  }\
  len+=res;\
getlen_0bit: ;\
}

static void unpack_c(int current_history_size, int history_size, uint8_t *src, uint8_t *dst, uint8_t *start, int left) {
  int ofs=-1;
  int len;
  uint32_t bits=0x80000000;
  uint32_t resbits;
  left--;
  history_size--;// becomes mask for circular buffer indexing
  if (current_history_size) {
    current_history_size-=dst-start;
    goto nextblock;
  }

copyletter:
  *dst++=*src++;
  left--;
nextblock:
  len=-1;

get_bit:
  if (left<0) return;
  if (getbit==0) goto copyletter;

  /* unpack lz */
  if (len<0) {
    len=1;
    if (!getbit) {
      goto uselastofs;
    }
  }
  len=2;
  getcode(bits,src,dst-start+current_history_size);
  ofs++;
  if (ofs>=longlen) len++;
  if (ofs>=hugelen) len++;
  ofs=-ofs;
uselastofs:
  getlen(bits,src);
  left-=len;

  int ptr = dst-start+ofs;
  do {
    *dst=start[ptr&(history_size)];
    ptr++;
    dst++;
  } while(--len);
  goto get_bit;
}

#ifdef ASM_X86
extern unsigned int unpack_x86(uint8_t *src, uint8_t *dst, int left);
#endif

#include "e8.h"
int main(int argc,char * argv[]) {
  int ifd,ofd;
  int n,n_unp;
  char shift;

  if (argc<3) {
    printf("usage: unpack input output\n  Unpacks file packed using lzoma algoritm\n");
    printf("Notice: this program is at experimental stage of development. Compression format is not stable yet.\n");
    exit(0);
  }

  ifd=open(argv[1],O_RDONLY|O_BINARY);
  ofd=open(argv[2],O_WRONLY|O_TRUNC|O_CREAT|O_BINARY,511);
  int current_history = 0;
  int ofs = 0;
  int use_e8=0;
  uint8_t header[8];
  read(ifd,header,8);
  if (header[0] != (AuthorID >> 8) ||
      header[1] != (AuthorID & 0xFF) ||
      header[2] != AlgoID[0] ||
      header[3] != AlgoID[1] ||
      header[4] != AlgoID[2] ||
      header[5] != AlgoID[3] ||
      header[6] != Version) {
    fprintf(stderr, "Unsupported compressed data format\n");
    return 1;
  }
  int dict_size = header[7] & 0xF;
  int history_size = HISTORY_SIZE(dict_size);
  int block_size = BLOCK_SIZE(dict_size);
  in_buf = (uint8_t *)malloc(block_size);
  out_buf = (uint8_t *)malloc(history_size); // history is 16*block_size

  uint32_t blk;
  while(read(ifd,&blk,4)==4) {
    //if (use_e8) e8(out_buf,n_unp);
    n = blk & (block_size-1);
    if (blk & BLOCK_STORED) {
      n_unp = n;
    } else if (blk & BLOCK_LAST) {
      read(ifd,&n_unp,4);
    } else {
      n_unp = block_size;
    }
    /*
    if (n != n_unp && !current_history) 
      read(ifd,&use_e8,1);
    else
      use_e8 = 0;
    */
    //long unsigned tsc = (long unsigned)__rdtsc();
    if (n == n_unp) {
      read(ifd,out_buf,n_unp);
      write(ofd,out_buf+ofs,n_unp);
    } else {
      read(ifd,in_buf,n);
#ifdef ASM_X86
#error Asm version not yet updated for recent format changes. Please use C version right now.
      unpack_x86(in_buf, out_buf, n_unp);
#else
      unpack_c(current_history, history_size, in_buf, out_buf+ofs, out_buf, n_unp);
#endif
      //tsc=(long unsigned)__rdtsc()-tsc;
      //printf("tsc=%lu\n",tsc);
      //if (use_e8) e8back(out_buf,n_unp);
      write(ofd,out_buf+ofs,n_unp);
    }
    if (blk & BLOCK_LAST)
      break;
    ofs+=n_unp;
    ofs &= (history_size-1);
    current_history += n_unp;
    if (current_history > history_size-block_size)
      current_history = history_size-block_size;
  }

  close(ifd);
  close(ofd);
  return 0;
}
