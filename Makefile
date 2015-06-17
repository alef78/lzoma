all:
	gcc -m32 -O2 -march=bonnell -pipe pack.c -o pack
	gcc -m32 -fomit-frame-pointer -std=c99 -Os -march=bonnell -pipe unpack.c unpack_lzoma.S -o unpack
test:
	rm -f v && time ./unpack vmlinux.p3 v && md5sum v vmlinux.bin
