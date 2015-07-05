
void e8(signed long n) {
  long i;
  long *op;
  for(i=0; i<n-5;) {
    byte b = in_buf[i];
    if ((b == 0xF) && (in_buf[i+1] & 0xF0) == 0x80) {
      i++;
      b = 0xe8;
    }
    i++;
    b &= 0xFE;
    if (b == 0xe8) {
       op = (long *)(in_buf+i);
       if (*op >= -i && *op < n-i) {
         *op += i;
       } else if ( *op >= n-i && *op < n ) {
         *op -= n; // to [-i,1] 
       }
       i+=4;
    }
  }
}

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
