/*
  comp.c     headerfile for quasistatic probability model

  (c) Michael Schindler
  1997, 1998, 1999, 2000
  http://www.compressconsult.com/
  michael@compressconsult.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.  It may be that this
  program violates local patents in your country, however it is
  belived (NO WARRANTY!) to be patent-free here in Austria.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  comp is an example compressor trying to compress files with a simple
  order 0 model. The files can be decompressed by decomp.

  Note that I do not think that an order 0 model as here is good;
  For better compression see for example my freeware szip.
  http://www.compressconsult.com/szip/
  or ask me as consultant what compression method fits your data best.
*/

#include <stdio.h>
#include <stdlib.h>
#ifndef unix
#include <io.h>
#include <fcntl.h>
#endif
#include <string.h>
#include <ctype.h>
#include "port.h"
#include "rangecod.h"

void usage()
{   fprintf(stderr,"comp [inputfile [outputfile]]\n");
    fprintf(stderr,"comp (c)1997.1998 Michael Schindler, michael@compressconsult\n"); 
    exit(1);
}

int main( int argc, char *argv[] )
{   int ch1,ch2,ch3,ch4, syfreq, ltfreq;
    rangecoder rc;
    //qsmodel qsm[48];
    int prop[48];

    if ((argc > 3) || ((argc>0) && (argv[1][0]=='-')))
        usage();

    if ( argc<1 )
        fprintf( stderr, "stdin" );
    else
    {   freopen( argv[1], "rb", stdin );
        fprintf( stderr, "%s", argv[1] );
    }
    if ( argc<2 )
        fprintf( stderr, " to stdout\n" );
    else
    {   freopen( argv[2], "wb", stdout );
        fprintf( stderr, " to %s\n", argv[2] );
    }
    fprintf( stderr, "%s\n", coderversion);

#ifndef unix
    setmode( fileno( stdin ), O_BINARY );
    setmode( fileno( stdout ), O_BINARY );
#endif

    /* make an alphabet with 257 symbols, use 256 as end-of-file */
#define SMALL 25
//#define SMALL 400
int j;
for(j=0;j<48;j++) prop[j]=32768;
//    initqsmodel(&qsm[j],2,12,200,NULL,1);

    start_encoding(&rc,0,0);

    /* do the coding */
    while (1)
    {   
        int len;
        len = 0;
        if ((ch1=getc(stdin))==EOF) break;
        if ((ch2=getc(stdin))==EOF) break;
        if ((ch3=getc(stdin))==EOF) break;
        if ((ch4=getc(stdin))==EOF) break;
        len = ch4; len<<=8;
        len += ch3; len<<=8;
        len += ch2; len<<=8;
        len += ch1; 
	//fprintf(stderr,"%d\n",len);
	int i=0;
        for(;;) {
	encbit(&rc,len&1,prop+i);i++;
//          qsgetfreq(&qsm[i],len&1,&syfreq,&ltfreq);
  //        encode_shift(&rc,syfreq,ltfreq,12);
    //      qsupdate(&qsm[i],len&1);
	  len>>=1;
//	  i++;
	  if (len==0) {
	encbit(&rc,1,prop+i);
     //       qsgetfreq(&qsm[i],1,&syfreq,&ltfreq);
      //      encode_shift(&rc,syfreq,ltfreq,12);
       //     qsupdate(&qsm[i],1);
	    break;
	  }
	encbit(&rc,0,prop+i);i++;
         //   qsgetfreq(&qsm[i],0,&syfreq,&ltfreq);
          //  encode_shift(&rc,syfreq,ltfreq,12);
         //   qsupdate(&qsm[i],0);
	 //   i++;
	    len--;
	}
    }
    /* write 256 as end-of-file */
//    qsgetfreq(&qsm1,SMALL,&syfreq,&ltfreq);
//    encode_shift(&rc,syfreq,ltfreq,12);

    done_encoding(&rc);

    return 0;
}
