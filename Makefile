all:
	gcc -O2 -pipe pack.c divsufsort.c -o pack
	gcc -Os -fomit-frame-pointer -std=c99 -Os -pipe unpack.c -o unpack

asm_x86:
	gcc -O2 -pipe pack.c divsufsort.c -o pack
	gcc -DASM_X86 -m32 -Os -fomit-frame-pointer -std=c99 -pipe unpack.c unpack_lzoma.S -o unpack

test:
	./pack pack.c pack.c.lzoma && ./unpack pack.c.lzoma pack.c.test && md5sum pack.c pack.c.test
