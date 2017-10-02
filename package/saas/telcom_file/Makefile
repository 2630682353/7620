CC=/opt/toolchains/crosstools-mips-gcc-4.6-linux-3.4-uclibc-0.9.32-binutils-2.21/usr/bin/mips-unknown-linux-uclibc-gcc
#CC=/opt/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-uclibc-gcc
SRC=$(wildcard MyPlugin/*.c)
all:
	rm MyPlugin/saas -rf
	$(CC) -o MyPlugin/saas $(wildcard MyPlugin/*.c) -lctsgw -L./MyPlugin/lib
	rm -rf saas.cpk
	tar -zcvf saas.cpk   MyPlugin/saas MyPlugin/saas.conf Info.plugin

	