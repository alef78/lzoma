// test file decompression using LZOMA algoritm
// (c) Alexandr Efimov, 2015
// License: GPL v2 or later

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <x86intrin.h>

#define O_BINARY 0
#define byte unsigned char
#include "lzoma.h"

byte in_buf[MAX_SIZE]; /* text to be encoded */
byte out_buf[MAX_SIZE];

//#define getbit (((bits=bits&0x7f? bits+bits :  (((unsigned)(*src++))<<1)+1)>>8)&1)
#define getbit ((bits=bits&0x7fffffff? (resbits=bits,bits+bits) :  (src+=4,resbits=*((uint32_t *)(src-4)),(resbits<<1)+1)),resbits>>31)
#define loadbit

#define getcode(bits, src, ptotal) {\
  int total = (ptotal);\
  ofs=0;\
  long int res=0;\
  int x=1;\
  int top=0;\
\
  /*if (total > 256) {*/\
     top=lzlow(total);\
     x=256;\
     res=*src++;\
  /*}*/\
\
  while (1) {\
    x+=x;\
    if (x>=total+top) break;\
    if (x & lzmagic)\
      top=lzshift(top);\
    /*if (x>=512) {*/\
      if (res<top) {  goto getcode_doneit;}\
      ofs-=top;\
      total+=top;\
      top+=top;\
    /*}*/\
    loadbit;\
    res+=res+getbit;\
  }\
  x-=total;\
  if (res>=x) { \
    loadbit; res+=res+getbit;\
    res-=x;\
  }\
getcode_doneit: \
  ofs+=res;\
  /*fprintf(stderr,"ofs=%d total=%d\n",ofs,ptotal);*/\
}

#define getlen(bits, src) {\
  long int res=0;\
  int x=1;\
  \
  if (getbit==0) {\
    len+=getbit;\
    goto getlen_0bit;\
  }\
  len+=2;\
  while (1) {  \
    x+=x;\
    loadbit;\
    res+=res+getbit;\
    loadbit;\
    if (getbit==0) break;\
    len+=x;\
  }\
  len+=res;\
getlen_0bit: ;\
}

static void unpack_c(int skip, byte *src, byte *dst, int left) {
  int ofs=-1;
  int len;
  uint32_t bits=0x80000000;
  uint32_t resbits;
  left--;

  if (skip) {
    dst+=skip;
    left-=skip;
    len=-1;
    goto get_bit;
  }

copyletter:
  *dst++=*src++;
  len=-1;
  left--;

get_bit:
  if (left<0) return;
  loadbit;
  if (getbit==0) goto copyletter;

  /* unpack lz */
  if (len<0) {
    len=1;
    loadbit;
    if (!getbit) {
      goto uselastofs;
    }
  }
  len=2;
  getcode(bits,src,dst-out_buf);
  ofs++;
  if (ofs>=longlen) len++;
  if (ofs>=hugelen) len++;
  ofs=-ofs;
uselastofs:
  getlen(bits,src);
  left-=len;

  // Note: on some platforms memcpy may be faster here
  do {
    *dst=dst[ofs];
    dst++;
  } while(--len);
  goto get_bit;
}

#ifdef ASM_X86
extern unsigned int unpack_x86(byte *src, byte *dst, int left);
#endif

#include "e8.h"
int main(int argc,char * argv[]) {
  int ifd,ofd;
  int n,n_unp;
  char shift;

  ifd=open(argv[1],O_RDONLY|O_BINARY);
  ofd=open(argv[2],O_WRONLY|O_TRUNC|O_CREAT|O_BINARY,511);
  int skip = 0;
  while(read(ifd,&n,4)==4) {
    if (skip)
      memcpy((void *)out_buf,(void *)(out_buf+MAX_SIZE/2), MAX_SIZE/2);
    read(ifd,&n_unp,4);
    int use_e8=0;
    if (!skip) 
      read(ifd,&use_e8,1);
    else
      use_e8 = 0;
    read(ifd,in_buf,n);
    long unsigned tsc = (long unsigned)__rdtsc();
#ifdef ASM_X86
    unpack_x86(in_buf, out_buf, n_unp);
#else
    unpack_c(skip, in_buf, out_buf, n_unp+skip);
#endif
    tsc=(long unsigned)__rdtsc()-tsc;
    printf("tsc=%lu\n",tsc);
    if (use_e8) e8back(out_buf,n_unp);
    write(ofd,out_buf+skip,n_unp);
    skip = MAX_SIZE / 2;
  }

  close(ifd);
  close(ofd);
  return 0;
}
