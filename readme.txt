Experimental packer based on new compression algoritm LZOMA).
(C)2015-2016 Alexandr Efimov <alef@webzavod.ru>

This code can be redistributed on GPL Version 2 License.
For commercial licenses or support please contact author.

Project goals:
  extremely fast in-place decompression (similar to LZO)
  but with high compression ratio (much better than LZO, GZIP, BZIP2)

Current results:

Compression ratio is much higher than gzip. And much much higher than LZO.
Decompression speed is similar to UCL (a bit slower that LZO, faster than
gzip, bzip2 etc).
Decompressor code length is less than 300 bytes.
Has special filter for x86 code.
Decompression can be done in-place and does not require additional memory.

Overall, the results a very good for "compress once, unpack often" tasks like
linux kernel & ramdisk, readonly compressed filesystems.

Comparison with other compression software:
Nearest competitors are zstd, brotli.
Other compressors/archives either decompress much slower or has much worse compression ratio.

Compression ratio on binary files (without effect of e8e8 filter),
from best to worst:
brotli, lzoma, zstd

Compression ratio on text files:
brotli, zstd, lzoma

Decompressor code size:
lzoma, zstd, brotli

Decompression speed, on x86-64:
zstd is about 2x faster, lzoma and brotli has similar speed.

Decompression speed, on Intel Atom tablet:
zstd and lzoma has similar speed, brotli is 4x slower.

Algorithm description.

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

Notes:

Algorithm is still experimental, compressed format is not final yet.

File format (WIP, not implemented yet):
1. Header
uint8_t[2] AuthorID 0xA1, 0xEF // this goes before AlgoID to avoid possible signature conflict with other LZ compressors
uint8_t[4] AlgoID 'L','Z','O','M'
uint8_t Version 0x00
uint8_t HistorySize (low 4 bits) || Flags
        where HistorySize is
        0: 32k
        1: 64k
        2: 128k
        3: 256k
        4: 512k
        5: 1M
        6: 2M
        7: 4M
        8: 8M
        9: 16M
        10:32M
        11:64M
        12:128M
        13:256M
        14:512M
        15: 1G
        BlockSize = HistorySize / 16

        Flags:
        0x10 - use filters, 1 byte filter type follows
               0x00 - x86
               0x01 - x86-64
               0x02 - arm
               0x03 - mips
               0x04 - 0xF - reserved
               0x10 - use delta filter
               0x20 - text/xml filter
               0x40 - reserved
               0x80 - reserved
        0x20 - encrypted file
               TODO: some compression header follows
        0x40 - digitally signed file (signature follows at the end of file)
        0x80 - reserved

2. Blocks
Blocks header is 4 bytes or more:
high bits masks:
0x80000000 - if set, it is a stored block
0x40000000 - last block, 4 byte unpacked length follows unless it is a stored block
             if not set, unpacked length assumed to be BLOCK_SIZE
0x20000000 - reserved
0x10000000 - reserved
low 28 bits = packed length up to 2^28, can be zero

3. uint32_t CRC

