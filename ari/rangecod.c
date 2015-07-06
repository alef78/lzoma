#define NOWARN

/*
  define EXTRAFAST for increased speed; you loose compression and
  compatibility in exchange.
*/
//#define EXTRAFAST 

#include <stdio.h>	
#include "port.h"
#include "rangecod.h"

void encbit(rangecoder *rc,int bit, int *prop)
{
        int prob=(*prop)>>3;
	
	int x=6;

	if (bit) {
        encode_shift(rc,8192-prob,prob,13);
	*prop-=(*prop)>>x;
	} else {
        encode_shift(rc,prob,0,13);
	*prop+=(65536-(*prop))>>x;
	}
	*prop&=~1;
	*prop|=bit;
}
/* SIZE OF RANGE ENCODING CODE VALUES. */

#define CODE_BITS 32
#define Top_value ((code_value)1 << (CODE_BITS-1))


/* all IO is done by these macros - change them if you want to */
/* no checking is done - do it here if you want it             */
/* cod is a pointer to the used rangecoder                     */
#define outbyte(cod,x) putchar(x)
#define inbyte(cod)    getchar()


#ifdef RENORM95
#include "renorm95.c"

#else
#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS-2) % 8 + 1)
#define Bottom_value (Top_value >> 8)

#ifdef NOWARN
#ifdef GLOBALRANGECODER
char coderversion[]="rangecoder 1.3 NOWARN GLOBAL (c) 1997-2000 Michael Schindler";
#else
char coderversion[]="rangecoder 1.3 NOWARN (c) 1997-2000 Michael Schindler";
#endif
#else    /*NOWARN*/
#ifdef GLOBALRANGECODER
char coderversion[]="rangecoder 1.3 GLOBAL (c) 1997-2000 Michael Schindler";
#else
char coderversion[]="rangecoder 1.3 (c) 1997-2000 Michael Schindler";
#endif
#endif   /*NOWARN*/
#endif   /*RENORM95*/


#ifdef GLOBALRANGECODER
/* if this is defined we'll make a global variable rngc and    */
/* make RNGC use that var; we'll also omit unneeded parameters */
static rangecoder rngc;
#define RNGC (rngc)
#define M_outbyte(a) outbyte(&rngc,a)
#define M_inbyte inbyte(&rngc)
#define enc_normalize(rc) M_enc_normalize()
#define dec_normalize(rc) M_dec_normalize()
#else
#define RNGC (*rc)
#define M_outbyte(a) outbyte(rc,a)
#define M_inbyte inbyte(rc)
#endif


/* rc is the range coder to be used                            */
/* c is written as first byte in the datastream                */
/* one could do without c, but then you have an additional if  */
/* per outputbyte.                                             */
void start_encoding( rangecoder *rc, char c, int initlength )
{   RNGC.low = 0;                /* Full code range */
    RNGC.range = Top_value;
    RNGC.buffer = c;
    RNGC.help = 0;               /* No bytes to follow */
    RNGC.bytecount = initlength;
}


#ifndef RENORM95
/* I do the normalization before I need a defined state instead of */
/* after messing it up. This simplifies starting and ending.       */
static Inline void enc_normalize( rangecoder *rc )
{   while(RNGC.range <= Bottom_value)     /* do we need renormalisation?  */
    {   if (RNGC.low < (code_value)0xff<<SHIFT_BITS)  /* no carry possible --> output */
        {   M_outbyte(RNGC.buffer);
            for(; RNGC.help; RNGC.help--)
                M_outbyte(0xff);
            RNGC.buffer = (unsigned char)(RNGC.low >> SHIFT_BITS);
        } else if (RNGC.low & Top_value) /* carry now, no future carry */
        {   M_outbyte(RNGC.buffer+1);
            for(; RNGC.help; RNGC.help--)
                M_outbyte(0);
            RNGC.buffer = (unsigned char)(RNGC.low >> SHIFT_BITS);
        } else                           /* passes on a potential carry */
#ifdef NOWARN
            RNGC.help++;
#else
            if (RNGC.bytestofollow++ == 0xffffffffL)
            {   fprintf(stderr,"Too many bytes outstanding - File too large\n");
                exit(1);
            }
#endif
        RNGC.range <<= 8;
        RNGC.low = (RNGC.low<<8) & (Top_value-1);
        RNGC.bytecount++;
    }
}
#endif


/* Encode a symbol using frequencies                         */
/* rc is the range coder to be used                          */
/* sy_f is the interval length (frequency of the symbol)     */
/* lt_f is the lower end (frequency sum of < symbols)        */
/* tot_f is the total interval length (total frequency sum)  */
/* or (faster): tot_f = (code_value)1<<shift                             */
void encode_freq( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f )
{	code_value r, tmp;
	enc_normalize( rc );
	r = RNGC.range / tot_f;
	tmp = r * lt_f;
	RNGC.low += tmp;
#ifdef EXTRAFAST
    RNGC.range = r * sy_f;
#else
    if (lt_f+sy_f < tot_f)
		RNGC.range = r * sy_f;
    else
		RNGC.range -= tmp;
#endif
}

void encode_shift( rangecoder *rc, freq sy_f, freq lt_f, freq shift )
{	code_value r, tmp;
	enc_normalize( rc );
	r = RNGC.range >> shift;
	tmp = r * lt_f;
	RNGC.low += tmp;
#ifdef EXTRAFAST
	RNGC.range = r * sy_f;
#else
	if ((lt_f+sy_f) >> shift)
		RNGC.range -= tmp;
	else  
		RNGC.range = r * sy_f;
#endif
}


#ifndef RENORM95
/* Finish encoding                                           */
/* rc is the range coder to be used                          */
/* actually not that many bytes need to be output, but who   */
/* cares. I output them because decode will read them :)     */
/* the return value is the number of bytes written           */
uint4 done_encoding( rangecoder *rc )
{   uint tmp;
    enc_normalize(rc);     /* now we have a normalized state */
    RNGC.bytecount += 5;
    if ((RNGC.low & (Bottom_value-1)) < ((RNGC.bytecount&0xffffffL)>>1))
       tmp = RNGC.low >> SHIFT_BITS;
    else
       tmp = (RNGC.low >> SHIFT_BITS) + 1;
    if (tmp > 0xff) /* we have a carry */
    {   M_outbyte(RNGC.buffer+1);
        for(; RNGC.help; RNGC.help--)
            M_outbyte(0);
    } else  /* no carry */
    {   M_outbyte(RNGC.buffer);
        for(; RNGC.help; RNGC.help--)
            M_outbyte(0xff);
    }
    M_outbyte(tmp & 0xff);
    M_outbyte((RNGC.bytecount>>16) & 0xff);
    M_outbyte((RNGC.bytecount>>8) & 0xff);
    M_outbyte(RNGC.bytecount & 0xff);
    return RNGC.bytecount;
}


/* Start the decoder                                         */
/* rc is the range coder to be used                          */
/* returns the char from start_encoding or EOF               */
int start_decoding( rangecoder *rc )
{   int c = M_inbyte;
    if (c==EOF)
        return EOF;
    RNGC.buffer = M_inbyte;
    RNGC.low = RNGC.buffer >> (8-EXTRA_BITS);
    RNGC.range = (code_value)1 << EXTRA_BITS;
    return c;
}


static Inline void dec_normalize( rangecoder *rc )
{   while (RNGC.range <= Bottom_value)
    {   RNGC.low = (RNGC.low<<8) | ((RNGC.buffer<<EXTRA_BITS)&0xff);
        RNGC.buffer = M_inbyte;
        RNGC.low |= RNGC.buffer >> (8-EXTRA_BITS);
        RNGC.range <<= 8;
    }
}
#endif


/* Calculate culmulative frequency for next symbol. Does NO update!*/
/* rc is the range coder to be used                          */
/* tot_f is the total frequency                              */
/* or: totf is (code_value)1<<shift                                      */
/* returns the culmulative frequency                         */
freq decode_culfreq( rangecoder *rc, freq tot_f )
{   freq tmp;
    dec_normalize(rc);
    RNGC.help = RNGC.range/tot_f;
    tmp = RNGC.low/RNGC.help;
#ifdef EXTRAFAST
    return tmp;
#else
    return (tmp>=tot_f ? tot_f-1 : tmp);
#endif
}

freq decode_culshift( rangecoder *rc, freq shift )
{   freq tmp;
    dec_normalize(rc);
    RNGC.help = RNGC.range>>shift;
    tmp = RNGC.low/RNGC.help;
#ifdef EXTRAFAST
    return tmp;
#else
    return (tmp>>shift ? ((code_value)1<<shift)-1 : tmp);
#endif
}


/* Update decoding state                                     */
/* rc is the range coder to be used                          */
/* sy_f is the interval length (frequency of the symbol)     */
/* lt_f is the lower end (frequency sum of < symbols)        */
/* tot_f is the total interval length (total frequency sum)  */
void decode_update( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f)
{   code_value tmp;
    tmp = RNGC.help * lt_f;
    RNGC.low -= tmp;
#ifdef EXTRAFAST
    RNGC.range = RNGC.help * sy_f;
#else
    if (lt_f + sy_f < tot_f)
        RNGC.range = RNGC.help * sy_f;
    else
        RNGC.range -= tmp;
#endif
}


/* Decode a byte/short without modelling                     */
/* rc is the range coder to be used                          */
unsigned char decode_byte(rangecoder *rc)
{   unsigned char tmp = decode_culshift(rc,8);
    decode_update( rc,1,tmp,(freq)1<<8);
    return tmp;
}

unsigned short decode_short(rangecoder *rc)
{   unsigned short tmp = decode_culshift(rc,16);
    decode_update( rc,1,tmp,(freq)1<<16);
    return tmp;
}


/* Finish decoding                                           */
/* rc is the range coder to be used                          */
void done_decoding( rangecoder *rc )
{   dec_normalize(rc);      /* normalize to use up all bytes */
}
