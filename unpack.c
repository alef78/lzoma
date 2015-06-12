// test file decompression using LZOMA algoritm
// (c) Alexandr Efimov, 2015
// License: GPL v2 or later

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define O_BINARY 0
#define byte unsigned char
#define MAX_SIZE 16384*1024
#define longlen 0x0D00
int breaklz=1024;
#define breaklen 2

byte in_buf[MAX_SIZE]; /* text to be encoded */
byte out_buf[MAX_SIZE];

//#define getbit (1 & (bits = (bits <= 3 ? (0x100|*src++) : (bits>>1))))
#define getbit (((bits=bits&0x7f? bits+bits :  (((unsigned)(*src++))<<1)+1)>>8)&1)
#define loadbit

#define getcode(bits, src, ptotal, pbreak_at) {\
  int total = (ptotal);\
  int break_at = pbreak_at; \
  ofs=0;\
  long int res=0;\
  int x=1;\
\
  if (break_at >= 256 && total >= 256) {\
     x=256;\
     res=*src++;\
  }\
\
  while (1) {\
    total-=x;\
    if (x-total>=0) break;\
    if (x>=break_at) {\
      loadbit;\
      if (getbit==0) goto getcode_doneit;\
      ofs+=x;\
      total-=x;\
      break_at<<=3;\
    }\
    total+=x;\
    x+=x;\
    loadbit;\
    res+=res+getbit;\
  }\
  if (res>=x-total) { loadbit; if (getbit) res+=total;}\
getcode_doneit: \
  ofs+=res;\
}

#define getlen(bits, src, left) {\
  long int total = left; \
  long int res=0;\
  int x=1;\
  \
  if (getbit==0) {\
    len+=getbit;\
    goto getlen_0bit;\
  }\
  len+=2;\
  if (total<2) goto getlen_0bit;\
  if (total==2) goto getlen_lastone;\
  while (1) {  \
    x+=x;\
    loadbit;\
    res+=res+getbit;\
    total-=x;\
    if (x-total>=0) break; \
    loadbit;\
    if (getbit==0) goto getlen_doneit;\
    len+=x;\
  }\
  if (res<x-total) goto getlen_doneit;\
getlen_lastone:\
  loadbit; if (getbit) res+=total;\
getlen_doneit: \
  len+=res;\
getlen_0bit: ;\
}

static void unpack_c(byte *src, byte *dst, int left) {
  int ofs=-1;
  int len;
  int bits=0x80;
  left--;

copyletter:
  *dst++=*src++;
  //was_letter=1;
  len=-1;
  left--;

get_bit:
  if (left<0) return;
  loadbit;
  if (getbit==0) goto copyletter;

  /* unpack lz */
  if (len<0) {
    len=2;
    loadbit;
    if (!getbit) {
      goto uselastofs;
    }
  }
  len=2;
  getcode(bits,src,dst-out_buf,breaklz);
  ofs++;
  if (ofs>=longlen) len++;
  ofs=-ofs;
uselastofs:
  getlen(bits,src,left);
//  printf("lz: %d:%d,left=%d\n",ofs,len,left);
  left-=len;
//    *dst=dst[ofs];
//    dst++;
//    --len;//len is at least 2 bytes - byte1 can be copied without checking
  //memcpy(dst,dst+ofs,len);dst+=len;
  do {
    *dst=dst[ofs];
    dst++;
    /*if (--len==0) goto get_bit;
    *dst=dst[ofs];
    dst++;
    if (--len==0) goto get_bit;
    *dst=dst[ofs];
    dst++;
    if (--len==0) goto get_bit;
    *dst=dst[ofs];
    dst++;*/
  } while(--len);
  /*if (ofs < -3)
  while(len>=4) {
    *(long*)dst=*(long*)(dst+ofs);
    dst+=4;
    len-=4;
  }
  while(len>0) {
    *dst=dst[ofs];
    dst++;
    len--;
  };*/
  goto get_bit;
}

extern void unpack(byte *src, byte *dst, int left);
void e8back(byte *buf,long n) {
  long i;
  signed long *op;
  for(i=0; i<n-5;) {
	byte b = buf[i];
    if ((b == 0xF) && (buf[i+1] & 0xF0) == 0x80) {
      i++;
      b = 0xe8;
    }
    b &= 0xFE;
    i++;

    if (b == 0xe8) {
       op = (long *)(buf+i);
       if (*op >= -i && *op < 0) {
         *op += n;
       } else if ( *op >= 0 && *op < n ) {
         *op -= i;
       }
       i+=4;
    } 
  }
}

int main(int argc,char * argv[]) {
  int ifd,ofd;
  int n,n_unp;
  char shift;

  ifd=open(argv[1],O_RDONLY|O_BINARY);
  ofd=open(argv[2],O_WRONLY|O_CREAT|O_BINARY,511);
  while(read(ifd,&n,4)==4) {
    read(ifd,&n_unp,4);
    //read(ifd,&shift,1);
    //breaklz=1<<shift;
    breaklz = 1<<9;
    read(ifd,in_buf,n);
    //for(int t=0;t<10;t++)
    unpack(in_buf, out_buf, n_unp);
    e8back(out_buf,n_unp);
    write(ofd,out_buf,n_unp);
  }

  close(ifd);
  close(ofd);
  return 0;
}
