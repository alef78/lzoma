Experimental packer based on new compression algoritm LZOMA).
(C)2015 Alexandr Efimov <alef@webzavod.ru>

This code can be redistributed on GPL Version 2 License.
For commercial licenses or support please contact author.

Project goals:
  extremely fast in-place decomression (similar to LZO)
  but with high compression ratio (much better than LZO, ZIP, GZIP, BZIP2)

Current results:

Compression ratio is much higher that gzip / bzip2. And much much
higher than LZO. Somewhat higher than LZHAM, that has similar goals. A bit worse than lzma/xz.
Decompression speed is similar to UCL (a bit slower that LZO, faster than
gzip, bzip2 etc).
Decompressor code length is about 350-400 bytes.
Has special filter for x86 code.
Decompression can be done in-place and does not require additional memory.

Overall, the results a very good for "compress once, unpack often" tasks like
linux kernel & ramdisk, readonly compressed filesystems.

Drawbacks:
Compression at maximal ratio is currently VERY slow and requires a lot of memory and full input data loaded in one chunk.
It is possible to implement a very fast compressor, but at the cost of reduced
compression ratio (which will be similar to UCL compression then).
It may be possible to adapt lzma code to implement a reasonably fast compression,
not sure how will affect compression ratio.

Algorith description.

Compressed format has some features similar to both LZO and LZMA.
Does not use range coding.
Special bit added to matches that follow literals, indicating to re-use
previous offset instead of always storing the offset for each match. 
This allows to more efficiently compress patterns like abcdEabcdFabcdGabc, as
offset will be stored only for first match.

This idea allows much higher compression than classical LZ algorithms but
compressor is much more complicated.

Compressed data format:
literal, item, ... item

Where:
literal is uncompressed byte aligned at byte boundary
item is:
1 bit flag (literal | match)
if flag is literal then literal follows

if flag is match then
  if previous item was literal
    1 bit flag==1: use previous offset for match
  if not use previous offset for match
    offset (encoded)
  len (encoded)




    





