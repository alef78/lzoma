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
    int prop[256];

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

int j;
for(j=0;j<256;j++) prop[j]=32768;

    start_encoding(&rc,0,0);
    /* do the coding */
    while (1)
    {   
        unsigned char len;
        if ((ch1=getc(stdin))==EOF) break;
        len = ch1; 
	//fprintf(stderr,"%d\n",len);
	int ctx=1;
        for(;ctx<256;) {
          encbit(&rc,len>>7,prop+ctx);
	  ctx+=ctx+(len>>7);
	  len+=len;
	}
    }
    done_encoding(&rc);

    return 0;
}
