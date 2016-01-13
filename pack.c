// test file compression based on lzoma algorith
// (c) 2015,2016 Alexandr Efimov
// License: GPL 2.0 or later
// Uses divsufsort library for faster initialization (thanks to xezz for suggestion), see divsufsort.h for its license details.
//
// Discussion thread: http://encode.ru/threads/2280-LZOMA
//
// Notes:
//
// Pros:
// Compression ratio is very good (much higher than lzo, ucl, gzip, bzip2).
// Decompression speed is very high (faster than gzip, much faster than bzip,lzham, lzma,xz)
// tiny decompressor code (asm version of decompress function less than 400 bytes)
//
// compressed data format is somewhere between lzo and lzma
// uses static encoding and byte-aligned literals and byte-aligned parts of match offset for decompression speed
//
// Cons:
// compressor is VERY slow. It is possible to implement faster compressor at the cost of some compression ratio.
// may be it is possible to adapt lzma compressor code.
//
// Other:
// Code of both compression/decompression utils is experimental.
// compressed data format is not stable yet.
// compressor source code is more like a ground for experiments, not a finished product yet.
// some commented out code was intended for experiments with Reduced-offset LZ, RLE step before LZ, LZX-style encoding of matches, various heuristics, etc.
//
#include <stdio.h>
#include <stdlib.h>
#include"divsufsort.h"

#include "lzoma.h"
#include "bpe.h"
#define MINOLEN 1
#define MINLZ 2

// faster settings
int level=3;
int short_match_level=25;
int match_level=200;
// "normal" settings
//#define level 3
//#define match_level 1000
// slow settings - typically 3x slower than "normal", but may be 10x slower on some files. tiny gains (0.1% or so) in compression ratio.
//#define level 5
//#define match_level 10000
// note: higher values, especially for match_level, are completely unpractical - will result in extremely slow compression and nearly zero improvments in compression ratio.

FILE *flzlit=NULL;
FILE *flit=NULL;
FILE *folz=NULL;
FILE *flen=NULL;
FILE *fdist=NULL;

/* todo: these should become a structure, malloced and given as paramter to functions to aviod global variables */
int shared_use_olz_and_sorted[MAX_SIZE];
int shared_olz_len_and_sorted_len[MAX_SIZE+1];
int shared_use_olz2_and_sorted_prev[MAX_SIZE];
int shared_olz_len2_and_sorted_next[MAX_SIZE];
int shared_cache_and_same[MAX_SIZE];
int shared_best_len_and_samelen[MAX_SIZE];
int shared_best_ofs_and_gen_same[MAX_SIZE];
int rle[MAX_SIZE+1];

byte in_buf[MAX_SIZE]; /* text to be encoded */
//byte out_buf[MAX_SIZE];
#define out_buf ((char *)rle)

#define cache shared_cache_and_same /* best possible result in bits if we start with lz or letter code */
#define best_ofs shared_best_ofs_and_gen_same /* best way to start, assuming we do not start with OLD OFFSET code */
#define best_len shared_best_len_and_samelen /* best way to start - match len, assuming we do not start with OLD OFFSET code */
#define use_olz shared_use_olz_and_sorted /* if not zero, repeat same offset after this number of literals */
#define olz_len (shared_olz_len_and_sorted_len+1) /* length of repeated lz */
#define use_olz2 shared_use_olz2_and_sorted_prev /* if not zero, repeat same offset after this number of literals after first repeat */
#define olz_len2 shared_olz_len2_and_sorted_next /* length of repeated lz */

#define same shared_cache_and_same /* pointer to previous match of at least 2 bytes. for checking nearby short matches */
#define samelen shared_best_len_and_samelen /* length of match between this and previous string pointed by "same" */
#define gen_same shared_best_ofs_and_gen_same /* temp buffer used during initialiazation only */

// sorted tree in order to quickly check long matches starting from the longest match
#define sorted shared_use_olz_and_sorted
#define sorted_len (shared_olz_len_and_sorted_len+1)
#define sorted_prev shared_use_olz2_and_sorted_prev
#define sorted_next shared_olz_len2_and_sorted_next

static inline int len_encode(int num,int total) {
  if (total<=256) return 8;//top=0;
  register int res=8;
  register int x=256;

  int top = lzlow(total);
  while (1) {
    x+=x;
    if (x>=total+top) break; /* only 1 bit to be outputted left */
      if (x & lzmagic)
        top=lzshift(top);
    if (x>=breaklz) {
      if (num<top) { goto doneit;}
      num+=top;
      total+=top;
      top+=top;
    }
    res++;
  }
  if (num>=x-total) { res++;}
doneit: 
  return res;
}

static inline int len_encode_ol(int num) {//num>=2
  if (num<4) return 2;// 00 01
  num-=2;
  #define SKEW 1
  return SKEW+((31-__builtin_clz(num))<<1);
}

static inline int len_encode_l(int num) {//num>=2
  if (num<4) return 2;// 00 01
  num-=2;
  //#define SKEW 1
  //  return SKEW+((31-__builtin_clz(num))<<1);
  if (num<4) return 1+2;// 0 0=2 1=3
  if (num<8) return 1+4; // 10 00=4 01=5 10=6 11=7
  if (num<16) return 1+6; // 110 xxx 8-15
  if (num<32) return 1+8;
  if (num<64) return 1+10;
  if (num<128) return 1+12;
  if (num<256) return 1+14;
  if (num<512) return 1+16;
  if (num<1024) return 1+18;
  if (num<2048) return 1+20;
  if (num<4096) return 1+22;
  if (num<8192) return 1+24;
  if (num<16384) return 1+26;
  if (num<32768) return 1+28;
  if (num<65536) return 1+30;
  if (num<131072) return 1+32;
  if (num<262144) return 1+34;
  if (num<524288) return 1+36;
  return 1+38;
}

static inline int len_encode_l_dec(int num) {//num>=2
  if (num<4) return num;// 0 0=2 1=3
  if (num<8) return num-3; // 10 00=4 01=5 10=6 11=7
  if (num<16) return num-7; // 110 xxx 8-15
  if (num<32) return num-15;
  if (num<64) return num-31;
  if (num<128) return num-63;
  if (num<256) return num-127;
  if (num<512) return num-255;
  if (num<1024) return num-511;
  if (num<2048) return num-1023;
  if (num<4096) return num-2047;
  if (num<8192) return num-4095;
  if (num<16384) return num-8191;
  if (num<32768) return num-16383;
  if (num<65536) return num-32767;
  if (num<131072) return num-65535;
  if (num<262144) return num-131071;
  if (num<524288) return num-262143;
  return num-524287;
}

int lastpos;
unsigned int bit_cnt;
int outpos;

static inline void putbit(int bit) {
  bit_cnt>>=1;
  if (bit_cnt==0) {
    lastpos=outpos;
    *(unsigned long*)(out_buf+lastpos)=0;
    outpos+=4;
    bit_cnt=0x80000000;
  }
  if (bit) *(unsigned long *)(out_buf+lastpos)|=bit_cnt;
}

int stlet=0;
int stlz=0;
int stolz=0;
int bitslzlen=0;
int bitsolzlen=0;
int bitslen=0;
int bitsdist=0;
int bitslit=0;

static inline void putenc(int num,int total, int break_at, int debug) {
  char bits[100];
  int res=0;
  int x=1;
  int obyte=0;
  if (fdist) fwrite(&num,1,4,fdist);
  //  if (total > 256 && break_at >= 256 && !debug)
  obyte=1;
  bits[0]=0;
  bits[1]=0;
  bits[2]=0;
  bits[3]=0;
  bits[4]=0;
  bits[5]=0;
  bits[6]=0;
  bits[7]=0;
  //if (!debug)  fprintf(stderr,"ofs=%d total=%d\n",num,total);

  int top=lzlow(total);
  //if (total<=256) top=0;
  while (1) {
    x+=x;
    if (x>=512&& x>=total+top) break; /* only 1 bit to be outputted left */
      if (x & lzmagic) 
        top=lzshift(top);
    if (x>=break_at) {
      //if (top==0) top=lzlow(top);
      if (num<top) {  goto doneit;}
      num+=top;
      total+=top;
      top+=top;
    }
    bits[res++]=2;
  }
  x-=total;
  if (num>=x) {
    num+=x;
    bits[res++]=2;
  }

doneit: 
  for(;res<8;res++) {
    bits[res++]=2;
  }
  for(x=res-1;x>=0;x--) {
    if (bits[x]==2) {
      bits[x]=num&1;
      num>>=1;
    }
  }
  if (obyte) {
  //printf("res=%d\n", res);
  byte b=0;
  for(x=0;x<8;x++) {
    if (debug) printf("%d",bits[x]);
    if (bits[x]) b|=128>>x;
  }
  if (debug) printf(" ");
  out_buf[outpos++]=b;
  for(;x<res;x++) {
    if (debug) printf("%d",bits[x]);
    putbit(bits[x]);
  }
  }
  else 
  for(x=0;x<res;x++) {
    if (debug) printf("%d",bits[x]);
    putbit(bits[x]);
  }
  bitsdist+=res;
}

static inline void putenc_l(int num, int break_at) {
  char bits[100];
  int res=0;
  int x=1;
  int obyte=0;
  if (flen) fwrite(&num,1,4,flen);
  //fprintf(stderr,"%c",num);
  //fprintf(stderr,"%c",num>>8);

  if (num==0) {bitslen+=2; putbit(0);putbit(0);return;}
  if (num==1) {bitslen+=2; putbit(0);putbit(1);return;}
  putbit(1);num-=2;bitslen++;

  while (1) {
    x+=x;
    if (x>=break_at) {
      if (num<(x>>1)) {bits[res++]=0; break;}
      bits[res++]=1;
      num-=x>>1;
    }
    bits[res++]=2;
  }

  for(x=res-1;x>=0;x--) {
    if (bits[x]==2) {
      bits[x]=num&1;
      num>>=1;
    }
  }
  for(x=0;x<res;x++) {
    putbit(bits[x]);
  }
  bitslen+=res;
}

int old_ofs=0;
int was_letter=1;

void initout(int start) {
  outpos = 0;
  if (start==1) {
    out_buf[outpos++]=in_buf[0];
  }
  old_ofs=0;
  bit_cnt=1;
  was_letter=1;
}

static inline int min(int a,int b) {
  return a<b? a:b;
}

static inline void put_lz(int offset,int length,int used) {
  if (flzlit) fprintf(flzlit,"%c",1);
  putbit(1); bitslzlen++;
  offset=-offset; /* 1.. */
  offset--; /* 0.. */
//fprintf(stderr,was_letter?"\nw":"n");
//printf("was %d old %d\n",was_letter, old_ofs==offset? 1 : 0);
  if (was_letter) { bitsolzlen++;
    was_letter=0;
    if (old_ofs==offset) {
//fprintf(stderr,"o%d\n",length);
    stolz++;
    if (folz) fprintf(folz,"%c",0);
      putbit(0);
      //putenc_l(0,breaklen);
      //length^=1;
      putenc_l(length-MINOLEN,breaklen);
      return;
    }
    if (folz) fprintf(folz,"%c",1);
    //putenc_l(0,breaklen);
    putbit(1);
    //length++;
  }
  length-=MINLZ;
  stlz++;
#if LZX
  int total=used;
  int h=min(used,hugelen);
  int l=min(used,longlen);
  total+=h+l;
  if (length==0) {
    putenc(offset,total,breaklz, 0);
  } else if (length==1) {
    putenc(offset+l,total,breaklz, 0);
  } else {
    putenc(offset+l+h,total,breaklz, 0);
    putenc_l(length-2,breaklen);
  }
#else
  if (offset+1>=longlen) { length--; }
  if (offset+1>=hugelen) { length--; }
  //static int avg=0;
  //avg+=offset;avg>>=1;
  //static int avgl=0;
  //avgl+=length;avgl>>=1;
  //fprintf(stderr,"%d\t%d\t%d\t%d\t%d\t%d\t%d\n",offset,length,used,avg,avgl,len_encode(offset,used),len_encode_l(length+2));
  //int save1=offset>old_ofs?1:0;
  putenc(offset/*-save1*/,used/*-1*/,breaklz, 0);
  putenc_l(length-MINLZ+2,breaklen);
#endif

  old_ofs=offset;
}

static inline void put_letter(byte b) {
  if (flzlit) fprintf(flzlit,"%c",0);
  if (flit) fprintf(flit,"%c",b);
  putbit(0); bitslzlen++;
  out_buf[outpos++]=b; bitslit+=8;
  was_letter++;
  stlet++;
}

//static inline int len_bpe(int pos)
//{
//  return 1+2+len_encode(bpe_rofs[pos],pos);
//}

static inline int len_lz(int offset, int length, int used) { // offset>=1, length>=2, 
                                                    // if offset=>0xD00  length>=3
  int res=1; /* 1 bit = not a letter */
#if LZX
  int l=min(used,longlen);
  int h=min(used,hugelen);
  used+=l+h;
  offset--;
  if (length==2)
    return res+len_encode(offset,used);
  if (length==3)
    return res+len_encode(offset+l,used);
  res+=len_encode(offset+l+h,used);
  res+=len_encode_l(length-MINLZ+2-2);
#else
  //if (length==2) return len_bpe(used);
// with bpe len is 3 or more
  //if (offset>=bpe_total[used]) { length--; }
  if (offset>=longlen) { length--; }
//  length--;
  if (offset>=hugelen) { length--; }
//  offset+=bpe_total[used];used+=bpe_total[used];

  offset--; // 0.. 

  res+=len_encode(offset,used);
  res+=len_encode_l(length-MINLZ+2);
#endif
  return res;
}

static inline int len_olz_minus_lz(int offset, int length, int used) { // offset>=1, length>=2, 
                                                    // if offset=>0xD00  length>=3
  int res=2;//because olz
  //int ll=(offset>=longlen) ? 1:0;
  //if (offset>=hugelen) ll=2;
   { res+=len_encode_ol(length+2-MINOLEN); }
  //offset--; /* 0.. */
  return res-len_lz(offset,length,used);
}

static inline int cmpstr(int src,int src2) {
  int res=0;
  int b;
  
//  printf("%d:%d:%d\n",src,src2,left);
  for(;;) {
    if (in_buf[src]!=in_buf[src2]) return res;
    b=rle[src2];
    if (!b) return res;
    if (b>rle[src]) {return res+rle[src];}
    res+=b;
    src+=b;
    src2+=b;
  }
  return res;
}

int cmpstrsort(int *psrc,int *psrc2) {
  int b;
  int src = *psrc;
  int src2 = *psrc2;
//  printf("%d:%d:%d\n",src,src2,left);
  do {
    if (in_buf[src]<in_buf[src2]) return -1;
    if (in_buf[src]>in_buf[src2]) return 1;
    b=rle[src2];
    if (!b) return 1; // first string is longer
    if (b>rle[src]) b=rle[src];
    if (!b) return -1; // second string is longer
    src+=b;
    src2+=b;
  } while(1);
}

void init_same(int start, int n) {
  int i;
  unsigned short int bb;
  int old;

  old = 1;
  rle[n-1]=old;
  byte b = in_buf[n-1];
  for(i=n-2;i>=0;i--) {
    if (in_buf[i]==b) 
      {/*if (old<255)*/ old++;}
    else {
      b=in_buf[i];
      old = 1;
    }
    rle[i]=old;
  }
  rle[n]=0;

  bb=0;old=0;

  for(i=0;i<65536;i++) {gen_same[i]=-1; }
  for(i=0;i<n-1;i++) { 
    cache[i]=0;
    bb=in_buf[i]; bb<<=8; bb|=in_buf[i+1];
    same[i]=gen_same[bb]; 
    if (gen_same[bb]>=0) { samelen[i]=1+cmpstr(i+1,same[i]+1);}
    gen_same[bb]=i; 
  }
  same[i]=-1;

  if (n > 1000) {
    for(i=0;i<256+256*256;i++) gen_same[i] =0;	// for bucketA & bucketB
    divsufsort(in_buf,sorted,gen_same,n);
  } else {
    for(i=0;i<n;i++) sorted[i]=i;
    qsort( sorted, n, sizeof( int ),
                 ( int (*)(const void *, const void *) ) cmpstrsort );
  }

  for(i=0;i<n-1;i++) sorted_len[sorted[i]]=cmpstr(sorted[i],sorted[i+1]);
  sorted_len[sorted[n-1]]=0;
  sorted_len[-1] = 0;

  sorted_prev[sorted[0]]=-1;
  for(i=1;i<n;i++) sorted_prev[sorted[i]]=sorted_len[sorted[i-1]] > MINLZ? sorted[i-1]:-1;

  for(i=0;i<n-1;i++) sorted_next[sorted[i]]=sorted_len[sorted[i]] > MINLZ? sorted[i+1]:-1;
  sorted_next[sorted[n-1]]=-1;
  in_buf[n]=0;
  
  printf("init done.\n");
}

static inline int max(int a,int b) {
  return a>b? a:b;
}

#define CHECK_OLZ \
        int k;\
	int jjj;\
        int d=level;\
        int tmp=len_lz(used-pos,len,used);\
        int olen=0;\
        for(k=len+1;k<left-2;k++) {\
          tmp+=9;\
          if (best_ofs[used+k]==pos-used) {\
            int tmp2=tmp+len_olz_minus_lz(used-pos,best_len[used+k],used+k);\
            tmp2+=cache[used+k];\
            if (tmp2<res || (tmp2==res && my_best_ofs<pos-used)) {\
              res=tmp2;\
              my_best_ofs=pos-used;\
              my_best_len=len;\
              my_use_olz=k-len;\
              my_olz_len=best_len[used+k];\
              my_use_olz2=use_olz[used+k];\
	      my_olz_len2=olz_len2[used+k];\
            }\
          }\
          if (olen==0) {\
            olen=cmpstr(used+k,pos+k);\
            for (j=MINOLEN;j<olen;j++) {\
              int tmp2=tmp+1+1+len_encode_ol(j+2-MINOLEN);\
              tmp2+=cache[used+k+j];\
              if (tmp2<res || (tmp2==res && my_best_ofs<pos-used)) {\
                res=tmp2;\
                my_best_ofs=pos-used;\
                my_best_len=len;\
                my_use_olz=k-len;\
                my_olz_len=j;\
                my_use_olz2=0;\
              }\
            }\
	    if (olen>=MINOLEN) {\
              int tmp2=tmp+2+len_encode_ol(olen+2-MINOLEN);\
              tmp2+=cache[used+k+olen];\
              if (best_len[used+k+olen]==1) {\
	        int jj;\
	        for(jj=1;jj<=8;jj++) {\
                  if (best_len[used+k+olen+jj]>1) {\
                    if (best_ofs[used+k+olen+jj]==pos-used) {\
                      tmp2+=len_olz_minus_lz(used-pos,best_len[used+k+olen+jj],used+k+olen+jj);\
		      break;\
                    }\
	          }\
                      int olen2=cmpstr(used+k+olen+jj,pos+k+olen+jj);\
            for (jjj=MINOLEN;jjj<=olen2;jjj++) {\
		     /* if (olen2>=MINOLEN) {*/\
		        int tmp3=-cache[used+k+olen];\
			tmp3+=jj*9+2+len_encode_ol(jjj+2-MINOLEN);\
			tmp3+=cache[used+k+olen+jj+jjj];\
			if (tmp3<0) { tmp3+=tmp2;\
              if (tmp3<res || (tmp3==res && my_best_ofs<pos-used)) {\
                res=tmp3;\
                my_best_ofs=pos-used;\
                my_best_len=len;\
                my_use_olz=k-len;\
                my_olz_len=olen;\
                my_use_olz2=jj;\
                my_olz_len2=jjj;\
              }\
			}\
		      }\
	            break;\
	        }\
              }\
              if (tmp2<res || (tmp2==res && my_best_ofs<pos-used)) {\
                res=tmp2;\
                my_best_ofs=pos-used;\
                my_best_len=len;\
                my_use_olz=k-len;\
                my_olz_len=olen;\
                my_use_olz2=0;\
              }\
	    }\
          } else olen--;\
          if (best_ofs[used+k]) {\
            d--; if (d==0) break;\
          }\
        }


int pack(int start, int n) {
  int res;
  int i;

  if (n<start) { return 0; }

  init_same(start,n);
  cache[n-1]=9; /* last letter cannot be packed as a lz */
  best_ofs[n-1]=0;
  best_len[n-1]=1;
  use_olz[n-1]=0;
  use_olz[n-1]=0;

  if (sorted_prev[n-1]>=0) {
    sorted_len[sorted_prev[n-1]] = min(sorted_len[sorted_prev[n-1]],
                                            sorted_len[n-1]);
    sorted_next[sorted_prev[n-1]]=sorted_next[n-1];
  }
  if (sorted_next[n-1]>=0) sorted_prev[sorted_next[n-1]]=sorted_prev[n-1];

  for(i=n-2;i>=start;i--) {
    int used=i;
    int left=n-i;
    int res;
    int pos;
    int max_match;
    int len;
    int j;

    int my_best_ofs=0;
    int my_best_len=1;
    int my_use_olz=0;
    int my_use_olz2=0;
    int my_olz_len=0;
    int my_olz_len2=0;
    int match_check_max;
    int notskip = 1; 

    res=9+cache[used+1];
    if (best_ofs[used+1]) {
      //int ll=-best_ofs[used+1] < longlen ? 0:1;
      //res+=len_encode_l(best_len[used+1]-ll+1)-len_encode_l(best_len[used+1]-ll);
      res++;
      //res++;
      if (in_buf[used]==in_buf[used+1]) {
        if (in_buf[used]==in_buf[used-1]) {
          if (in_buf[used]==in_buf[used+best_ofs[used+1]]) {
            if ((best_len[used+1]>3)||(best_len[used+1]==3&&-best_ofs[used+1]<hugelen)|| (-best_ofs[used+1]<longlen)) {
              int tmp=cache[used+1]-len_lz(-best_ofs[used+1],best_len[used+1],used+1)
                  +len_lz(-best_ofs[used+1],best_len[used+1]+1,used);
              if (tmp<=res) {
                res=tmp;
                my_best_ofs=best_ofs[used+1];
                my_best_len=best_len[used+1]+1;
                my_use_olz=use_olz[used+1];
                my_olz_len=olz_len[used+1];
                my_use_olz2=use_olz2[used+1];
                my_olz_len2=olz_len2[used+1];
              }
              if (my_best_len>=5)
                notskip = 0;
            }
          }
        }
      }
    }

    int k;
    for(k=1;k<4;k++)
      if (n-i>2+k && best_ofs[used+2+k] && -best_ofs[used+2+k] < longlen && best_ofs[used+2+k]!=best_ofs[used+1+k]) {
        if (in_buf[used]==in_buf[used+best_ofs[used+2+k]]) {
          if (in_buf[used+1]==in_buf[used+1+best_ofs[used+2+k]]) {
              int tmp=cache[used+2+k]+len_olz_minus_lz(-best_ofs[used+2+k],best_len[used+2+k],used+2+k)
                  +9*k+len_lz(-best_ofs[used+2+k],2,used);
	      if (tmp<=res) {
	        res=tmp;
                my_best_ofs=best_ofs[used+2+k];
                my_best_len=2;
                my_use_olz=k;
                my_olz_len=best_len[used+2+k];
                my_use_olz2=use_olz[used+2+k];
                my_olz_len2=olz_len[used+2+k];
	      }
          }
        }
      }

    for(k=1;k<4;k++)
      if (n-i>3+k && best_ofs[used+3+k] && -best_ofs[used+3+k] < hugelen && best_ofs[used+3+k]!=best_ofs[used+2+k]) {
        if (in_buf[used]==in_buf[used+best_ofs[used+3+k]]) {
          if (in_buf[used+1]==in_buf[used+1+best_ofs[used+3+k]]) {
            if (in_buf[used+2]==in_buf[used+2+best_ofs[used+3+k]]) {
              int tmp=cache[used+3+k]+len_olz_minus_lz(-best_ofs[used+3+k],best_len[used+3+k],used+3+k)
                  +9*k+len_lz(-best_ofs[used+3+k],3,used);
	      if (tmp<=res) {
	        res=tmp;
                my_best_ofs=best_ofs[used+3+k];
                my_best_len=3;
                my_use_olz=k;
                my_olz_len=best_len[used+3+k];
                my_use_olz2=use_olz[used+3+k];
                my_olz_len2=olz_len[used+3+k];
	      }
            }
          }
        }
      }

    for(k=1;k<4;k++)
      if (n-i>4+k && best_ofs[used+4+k] && best_ofs[used+4+k]!=best_ofs[used+3+k]) {
        if (in_buf[used]==in_buf[used+best_ofs[used+4+k]]) {
          if (in_buf[used+1]==in_buf[used+1+best_ofs[used+4+k]]) {
            if (in_buf[used+2]==in_buf[used+2+best_ofs[used+4+k]]) {
              if (in_buf[used+3]==in_buf[used+3+best_ofs[used+4+k]]) {
                int tmp=cache[used+4+k]+len_olz_minus_lz(-best_ofs[used+4+k],best_len[used+4+k],used+4+k)
                  +9*k+len_lz(-best_ofs[used+4+k],4,used);
	        if (tmp<=res) {
	          res=tmp;
                  my_best_ofs=best_ofs[used+4+k];
                  my_best_len=4;
                  my_use_olz=k;
                  my_olz_len=best_len[used+4+k];
                  my_use_olz2=use_olz[used+4+k];
                  my_olz_len2=olz_len[used+4+k];
	        }
              }
            }
          }
        }
      }
    pos=same[used];
    if (pos<0) goto done;

    if (notskip) {
      len=samelen[used];
      int ll=(used-pos>=longlen)?1:0;
      if (used-pos>=hugelen) ll=2;
      if (len<left && len>=2+ll) {
        CHECK_OLZ
      }
      for(j=MINLZ+ll;j<=len;j++) {
      //for(j=len;j>=max_match;j-=len_encode_l_dec(j-ll)) {
        int tmp=len_lz(used-pos,2-MINLZ+j,used);
        tmp+=cache[used+j];
        if (tmp<res) {
          res=tmp;
          my_best_ofs=pos-used;
          my_best_len=j;
          my_use_olz=0;
          my_olz_len=0;
        }
      }
      max_match=len;

    }
    if (max_match<MINLZ) max_match=MINLZ;
    match_check_max = short_match_level;
    if (notskip) 
      for(;;) {
        int slen=samelen[pos];
        pos=same[pos];
        if (used-pos>=longlen) break;
        if (pos<0) break;
        if (len>slen) {
          len=slen;
        } else if (len==slen) {
          len+=cmpstr(used+len,pos+len);
        } 
        if (len<left && len>=2) {
          CHECK_OLZ
        }
        if (len>max_match) {
          for(j=max_match+1;j<=len;j++) {
            int tmp=len_lz(used-pos,j-MINLZ+2,used);
            tmp+=cache[used+j];
            if (tmp<res) {
              res=tmp;
              my_best_ofs=pos-used;
              my_best_len=j;
              my_use_olz=0;
              my_olz_len=0;
            }
          }
	  max_match=len;
        } else { match_check_max--; if (match_check_max <= 0) break; }
        
      }
    if (!notskip) goto done;
    
    max_match=my_best_len+1;//2;
    if (max_match<MINLZ) max_match=MINLZ;
    int top=sorted_prev[used];
    int bottom=sorted_next[used];
    int len_top=sorted_len[top];
    int len_bottom=sorted_len[used];

    match_check_max = match_level;
    int my_min_ofs=used+1;
    while (top>=0 || bottom >=0) {
      match_check_max--;
      if (match_check_max<=0) goto done;
      if (len_top>len_bottom) {
        pos=top;
	len=len_top;
	top=sorted_prev[pos];
        len_top = min(len_top,sorted_len[top]);
      } else {
        pos=bottom;
	len=len_bottom;
        len_bottom = min(len_bottom,sorted_len[bottom]);
	bottom=sorted_next[pos];
      }
//      if (used-pos<longlen) continue; // we already checked it
      if (len<=MINLZ) goto done;
      if (len<=MINLZ+1 && used-pos>=hugelen) continue; // 
      if (len<left) {
          CHECK_OLZ
      }
      if (my_min_ofs>used-pos) {
        my_min_ofs=used-pos;//we are checking matches in decreasing order. we need to check next matches only if those are shorter
        int ll=(used-pos>=hugelen)?1:0;
        for(j=MINLZ+1+ll;j<=len;j++) {
          int tmp=len_lz(used-pos,j-MINLZ+2,used);
          tmp+=cache[used+j];
          if (tmp<res || (tmp==res && my_best_ofs<pos-used)) {
            res=tmp;
            my_best_ofs=pos-used;
            my_best_len=j;
            my_use_olz=0;
            my_olz_len=0;
          }
        }
      }

    }    

done:
    if (sorted_prev[used]>=0) {
      sorted_len[sorted_prev[used]] = min(sorted_len[sorted_prev[used]],
                                            sorted_len[used]);
      sorted_next[sorted_prev[used]]=sorted_next[used];
    }
    if (sorted_next[used]>=0) {
      sorted_prev[sorted_next[used]]=sorted_prev[used];
    }

    best_ofs[used]=my_best_ofs;
    best_len[used]=my_best_len;
    use_olz[used]=my_use_olz;
    olz_len[used]=my_olz_len;
    use_olz2[used]=my_use_olz2;
    olz_len2[used]=my_olz_len2;
    cache[used]=res;

    if ((i&0xFFF)==0) {
      printf("\x0D%d left ",i);
      fflush(stdout);
    }
  }

  res=8+cache[start];
  printf("\nres=%d\n",res);
  res+=7;
  res>>=3;
  printf("res bytes=%d\n",res);
  if (res>=n-start) {
    return n;
  };

  /* now we can easily generate compressed stream */
  initout(start);
  for(i=start;i<n;) {
    if (best_len[i]==1) {
      put_letter(in_buf[i]); i++;
    } else {
      int k,ofs,len,k2,len2;
dolz:
//      printf("do_lz %d:%d,left=%d\n",best_ofs[i],best_len[i],n-i);
      put_lz(best_ofs[i],best_len[i],i);
      ofs=best_ofs[i];
      len=olz_len[i];
      len2=olz_len2[i];
      k=use_olz[i];
      k2=use_olz2[i];
      i+=best_len[i];
      if (k>0) {
        for(;k>0;k--) put_letter(in_buf[i++]);
        if ((use_olz[i])&&(len==best_len[i])&&(ofs==best_ofs[i])) goto dolz;
//        printf("put_lz %d:%d,left=%d\n",ofs,len,n-i);
        put_lz(ofs,len,i);
        i+=len;
      if (k2>0) {
        for(;k2>0;k2--) put_letter(in_buf[i++]);
        if ((use_olz[i])&&(len2==best_len[i])&&(ofs==best_ofs[i])) goto dolz;
//        printf("put_lz %d:%d,left=%d\n",ofs,len,n-i);
        put_lz(ofs,len2,i);
        i+=len2;
	
      }
	
      }
    }
  }
  printf("out bytes=%d\n",outpos);
  return outpos;
}

#include "e8.h"
int main(int argc,char *argv[]) {
  FILE *ifd,*ofd;
  int n,i,bres,blz;
  byte b;

  if (argc<3) {
    printf("usage: lzoma [OPTION] input output [lzlit lit olz len dist]\n\t-1 .. -9 Compression level\n");
    printf("Notice: this program is at experimental stage of development. Compression format is not stable yet.\n");
    if (argc>1 && argv[1][0]=='%') { // undocumented debug feature to check correctness of offset encoding, when tuning parameters in lzoma.h
      int i;
      int total=atoi(argv[1]+1);//16*1024*1024;
      for(i=total-10;i<total;i++) {
        printf("%04d:",i);
        putenc(i, total,breaklz, 1);
        printf("\n");
      }
    }
    exit(0);
  }
  int arg=1;
  int metalevel = 7;
  if (argv[arg][0]=='-') {
    if (argv[arg][1]>='1' && argv[arg][1]<='9')
      metalevel = argv[arg][1]-'0';
    arg++;
  }
  int levels[9][3]= {
    {1,1,1},
    {1,2,2},
    {2,3,5},
    {3,5,15},
    {3,7,30},
    {3,10,100},
    {3,20,200},
    {3,40,500},
    {3,100,1000}
  };
  metalevel--;
  level=levels[metalevel][0];
  short_match_level=levels[metalevel][1];
  match_level=levels[metalevel][2];
  char *inf=argv[arg++];
  char *ouf=argv[arg++];
  ifd=fopen(inf,"rb");
  ofd=fopen(ouf,"wb");
  if (arg<argc) flzlit=fopen(argv[arg++],"wb");
  if (arg<argc) flit=fopen(argv[arg++],"wb");
  if (arg<argc) folz=fopen(argv[arg++],"wb");
  if (arg<argc) flen=fopen(argv[arg++],"wb");
  if (arg<argc) fdist=fopen(argv[arg++],"wb");
  n=0;
  for(;;) {
    if (n==0) {
      n=fread(in_buf,1,MAX_SIZE,ifd);
      if (n<=0) break;
      printf("got %d bytes, packing %s into %s...\n",n,inf,ouf);
      int b1=cnt_bpes(in_buf,n);
      int use_e8=1;
      e8(n);
      int b2=cnt_bpes(in_buf,n);
      printf("stats noe8 %d e8 %d\n",b1,b2);
      if (b2<=b1) {
        use_e8=0;
        printf("reverted e8\n");

        e8back(in_buf,n);
      }

      bres=pack(1,n);
      if (bres==n) {
        fwrite(&n,4,1,ofd);
        fwrite(&n,4,1,ofd);
        fwrite(in_buf,1,n,ofd);
      } else {
        fwrite(&bres,4,1,ofd);
        fwrite(&n,4,1,ofd);
        fwrite(&use_e8,1,1,ofd);
        fwrite(out_buf,1,bres,ofd);
        //  for (i=0;i<n-1;i++) {printf("%d%s\n",cache[i],(cache[i]>=cache[i+1])?"":" !!!");};
      }
    } else { // next blocks
      memcpy(in_buf, in_buf+MAX_SIZE/2, MAX_SIZE/2);
      n=fread(in_buf+MAX_SIZE/2,1,MAX_SIZE/2,ifd);
      if (n<=0) break;
      printf("got %d bytes, packing...\n",n);
      bres=pack(MAX_SIZE/2,MAX_SIZE/2+n);
      if (bres==n) {
        fwrite(&n,4,1,ofd);
        fwrite(&n,4,1,ofd);
        fwrite(in_buf,1,n,ofd);
      } else {
        fwrite(&bres,4,1,ofd);
        fwrite(&n,4,1,ofd);
        fwrite(out_buf,1,bres,ofd);
        //  for (i=0;i<n-1;i++) {printf("%d%s\n",cache[i],(cache[i]>=cache[i+1])?"":" !!!");};
      }
    }
  }
  printf("closing files let=%d lz=%d olz=%d\n",stlet,stlz,stolz);
  printf("bits lzlit=%d let=%d olz=%d match=%d len=%d\n",bitslzlen,bitslit,bitsolzlen,bitsdist,bitslen);
  close(ifd);
  close(ofd);
  return 0;
}
