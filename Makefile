all:
	gcc -O2 -march=bonnell -pipe pack.c -o pack
	gcc -fomit-frame-pointer -std=c99 -Os -march=bonnell -pipe unpack.c unpack_lzoma.S -o unpack
test:
	rm -f v && time ./unpack vmlinux.p3 v && md5sum v vmlinux.bin
