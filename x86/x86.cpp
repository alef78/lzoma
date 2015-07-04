typedef unsigned int uint;
typedef unsigned char  uc;

#include <stdio.h>
#include <stdlib.h>

uint BytesLoaded;

uint flen( FILE* f )
{
  fseek( f, 0, SEEK_END );
  uint len = ftell(f);
  fseek( f, 0, SEEK_SET );
  return len;
}

void* fload( char* fname )
{
  FILE* temp = fopen(fname,"rb");
  if (temp==0) return 0;
  unsigned int len = flen(temp);
  BytesLoaded = len;
  char* buf = new char[len];
  fread( buf, len, 1, temp );
  fclose( temp );
  return buf;
}

void fsave( void* buf, unsigned int len, char* fname )
{
  FILE* temp = fopen(fname,"wb");
  fwrite( buf, len, 1, temp );
  fclose( temp );
}

uint fgetd( FILE* file)
{
  return fgetc(file)+(fgetc(file)<<8)+(fgetc(file)<<16)+(fgetc(file)<<24);
}

uint fgetw( FILE* file)
{
  return fgetc(file)+(fgetc(file)<<8);
}

void fputd( uint c, FILE* file )
{
  fputc( c    , file );
  fputc( c>> 8, file );
  fputc( c>>16, file );
  fputc( c>>24, file );
}


void fputw( uint c, FILE* file )
{
  fputc( c    , file );
  fputc( c>> 8, file );
}

#define wswap(a) ( ((a)>>8) + (((a)&255)<<8) )
#define bswap(a) ( wswap((a)>>16)+(wswap((a)&65535)<<16) )
//#define _bsw(a,i,h) (((uc(&)[4])(a))[i]<<(h))
//#define bswap(a) ( _bsw(a,0,24)+_bsw(a,1,16)+_bsw(a,2,8)+_bsw(a,3,0) )

#include <map>
std::map<int,int> cofs;
std::map<int,int> jofs;
#define mask 0xfffff800
#define shift 0x400
int main(int argc,char* argv[])
{
int cn=0;
int jn=0;
  int i,j,k,len; uint a,b;

  uc* p; uc* q;
  FILE* Codes = fopen("main.dat","wb");
  FILE* Calls = fopen("calls.dat","wb");
//  FILE* Calls2 = fopen("calls2.dat","wb");
  FILE* Jumps = fopen("jumps.dat","wb");
  FILE* Flags = fopen("flags.dat","wb");

  uc* Text = (uc*)fload(argv[1]); p=Text;

  for( i=0; i<BytesLoaded; i++ )
  { 
    if ( p[i]==0xE8 && i<=BytesLoaded-5-3 )  
    {
      a = i + (uint&)p[i+1];

      if ( a<BytesLoaded ) {
        a+=shift;
        a&=mask;
        if (cofs.count(a)) cofs[a]++;
	else cofs[a]=1;
        i+=4;
      }
    }

    if ( p[i]==0xE9 && i<=BytesLoaded-5-3 ) {
      a = i + (uint&)p[i+1];
      if ( a<BytesLoaded ) {
        a+=shift;
        a&=mask;
        if (jofs.count(a)) jofs[a]++;
	else jofs[a]=1;
        i+=4;
      }

    } 

    if ( p[i]==0x0F && (p[i+1]&0xF0)==0x80 && i<=BytesLoaded-6 ) {
      a = i + (uint&)p[i+2];
      if ( a<BytesLoaded ) {
        a+=shift;
        a&=mask;
        if (jofs.count(a)) jofs[a]++;
	else jofs[a]=1;
        i+=4;
      }

    } 
 
  }
  for( i=0; i<BytesLoaded; i++ )
  { 

    fputc( p[i], Codes );

    if ( p[i]==0xE8 && i<=BytesLoaded-5-3 )  
    {
      a = i + (uint&)p[i+1];
//result becomes in [0..n)
//initially we are at [-i..n-i)
//if (a<BytesLoaded+i) {
      if ( a<BytesLoaded && cofs.count((a+shift)&mask) && cofs[(a+shift)&mask]>1 ) {
        putc( 0x00, Flags );

          fputd( bswap(a), Calls );

//        if ( p[i+5]==0x83 && p[i+6]==0xC4 && p[i+7]>0 ) {
//         fputc( 1/*p[i+7]*/, Calls2 );
//         i+=2;//3;
//        } else {
//         fputc( 0x00, Calls2 );
//        }

        i+=4;
      } else {
        putc( 0x01, Flags );
      }
//      }
    }

    if ( p[i]==0xE9 && i<=BytesLoaded-5-3 ) {
      a = i + (uint&)p[i+1];
//if (a<BytesLoaded+i) {
      if ( a<BytesLoaded && jofs.count((a+shift)&mask) && jofs[(a+shift)&mask]>1 ) {
        putc( 0x00, Flags );
        fputd( bswap( a ), Jumps );
        i+=4;
      } else {
        putc( 0x01, Flags );
      }
//}
    } 

    if ( p[i]==0x0F && (p[i+1]&0xF0)==0x80 && i<=BytesLoaded-6 ) {
      a = i + (uint&)p[i+2];
//if (a<BytesLoaded+i) {
      if ( a<BytesLoaded && jofs.count((a+shift)&mask) && jofs[(a+shift)&mask]>1 ) {
        putc( 0x00, Flags );
        fputd( bswap( a ), Jumps );
        i+=4;
      } else {
        putc( 0x01, Flags );
      }
//}
    } 
 
  }
  printf("\n");
 
}

