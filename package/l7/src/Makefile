
SRC=$(wildcard *.c)
target=l7.tar.gz
target_org=l7_org.tar.gz
openssl_dir=./openssl-1.0.1j/
dep_include=-I$(openssl_dir)/include/ -I../../libshare/src/include/
dep= ./libcrypto.a
CC=mipsel-openwrt-linux-gcc
STRIP=mipsel-openwrt-linux-strip
HOSTCC=gcc
cpu=$(shell uname -m)
ssl=./libcrypto_x86.a
ifeq ($(cpu), x86_64)
	ssl=./libcrypto_x86_64.a
endif

all: encpt
	$(CC) -O3 -o l7_load l7_load.c $(CFLAGS) $(LDFLAGS) -lshare $(dep) $(dep_include)
	$(STRIP) l7_load
	$(CC) -O3 -o l7_load_txt l7_load_txt.c $(CFLAGS) $(LDFLAGS) -lshare $(dep) $(dep_include)
	$(STRIP) l7_load_txt

encpt:
	$(HOSTCC) -o enc enc.c $(ssl) $(dep_include)
	rm -rf 1 2 3 4 10000 $(target) $(target_org)

clean:
	rm -rf *.o l7_load l7_load_txt $(target)
