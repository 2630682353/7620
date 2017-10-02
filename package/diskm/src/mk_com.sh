#!/bin/sh
export PATH="/home/zhangwei/Kingston/openwrt/7628/trunk/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin":$PATH
#/home/zhangwei/Kingston/openwrt/7628/trunk/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/lib/
mipsel-openwrt-linux-gcc upnpd.c -o kupnpd -I./ -L"../../openwrt/7628/trunk/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/lib/" -lshare -luci -lubox
