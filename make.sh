#!/bin/bash
if [ $# -lt 2 ]
then 
	echo "./make.sh BLINK BLINK"
	echo "./make.sh BLINK BL-WR328         PS:for 4+32 7620 no ali  suning"
	echo "./make.sh BLINK WR88N            PS:for 4+32 ali"
	echo "./make.sh BLINK WR308            PS:for 4+32 ali"
	echo "./make.sh BLINK WR316            PS:for 4+32 no ali"
	echo "./make.sh GAOKE Q339"
	echo "./make.sh GAOKE W307             PS:for 4+32 7620 del ali"
	echo "./make.sh GAOKE Q331             PS:for 4+32 7620 del ali"
	echo "./make.sh GAOKE Q307R"
	echo "./make.sh GAOKE Q338"
	echo "./make.sh GAOKE W305"
	echo "./make.sh GAOKE Q363"
	echo "./make.sh GAOKE Q363A			   PS:for 4+32 no ali"
	echo "./make.sh GAOKE W310"
	echo "./make.sh GAOKE Q335             PS:for 4+32 7620 no ali"
	echo "./make.sh GAOKE Q307R_V2         PS:for 4+32 7620 no ali"
	echo "./make.sh GAOKE Q363A            PS:for 4+32 7620 no ali"
	echo "./make.sh WIAIR I1               PS:for 8+64 7620 no usb"
	echo "./make.sh WIAIR I1U              PS:for 8+64 7620 usb no ali"
	echo "./make.sh PHICOMM PSG1208"
	echo "./make.sh ZHIBOTONG ZYJXA-1S     PS:for 4+64"
	echo "./make.sh ZHIBOTONG ZYJXB-1H     PS:for 8+64 usb"
	echo "./make.sh ZHIBOTONG WE26         PS:for 8+64 no ali no usb"
	echo "./make.sh ZHIBOTONG RN401        PS:for 8+64"
	echo "./make.sh DEMO D864              PS:for 8+64"
	echo "./make.sh DEMO D432              PS:for 4+32"
	echo "./make.sh ANTBANG A3             PS:for 4+32 ali"
	echo "./make.sh ANTBANG A3S            PS:for 4+32 ali_baby"
	echo "./make.sh ANTBANG A3C            PS:for 4+32 no ali"
	echo "./make.sh ANTBANG A5C            PS:for 4+32 no ali"
	echo "./make.sh TUOSHI D1"
	echo "./make.sh KZL WR507              PS:for 8+64 no ali usb"
	echo "./make.sh SZXS I1                PS:for 8+64 szxs"
	echo "./make.sh MEISIQI WQ-300         PS:for 4+32 no ali no usb"
	echo "./make.sh JCC CD8600             PS:8+64 no ali usb"
	echo "./make.sh EDUP V1                PS:8+64 no ali usb"
	echo "./make.sh HUITEFONE 8305S        PS:4+64 no ali"
	echo "./make.sh YOCAWIFI 1S            PS:8+64 no ali"
	echo "./make.sh KAER KL100             PS:4+32 no ali"
	echo "./make.sh WFYL WF_1620A          PS:8+64 no ali"
	echo "GPIO SUPPORT: 40 41 42 43 44 38 39 72"
	exit -1
fi

bridgemode=0
file=./package/ioos/src/version
out=./package/ioos/src/firmware
mu=./package/mu/files/mu
kdir=./kernel/linux/
date=`date '+%F %H:%M'`
wanpid=4
reset=1
vendor=$1
product=$2
AC=$3
lanip=192.168.1.1
domain=wiair.cc
ssid=CYWiFi-
telnet_port=2323
firmware=/dev/mtd4
www_dir=www_blink
login_user=admin
login_pwd=admin
flash_size=4M
gcc_define=
region=1
aregion=5
config=config_4+32
kconfig=$kdir/config_4+32
product_file=product_4_32.h
timezone=CST-8
model=
ruser=
rpwd=
wuser=
wpwd=
reset_time=3
gpio_power=38
gpio_wan=44
gpio_lan1=43
gpio_lan2=42
gpio_lan3=41
gpio_lan4=40
gpio_internet=
gpio_wifi24g=72
gpio_wifi5g=
gpio_wps=
gpio_usb=
retime=
svn revert $mu

export VENDOR=$vendor
export PRODUCT=$product

case "$vendor" in 
	"BLINK")
	wanpid=4
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	cp -rf uboot_blink_32m_sdr.bin uboot.bin
	cp -rf factory_blink.bin factory.bin
	lanip=192.168.16.1
	domain=blinklogin.cn
	ssid=B-LINK_
	reset=2
	if [ "$PRODUCT" = "$VENDOR" ]; then
		PRODUCT=mt7620_4+32
	elif [ "$PRODUCT" = "WR316" ]; then
		config=config_4+32_no_ali
		www_dir=www_blink_wr316
	elif [ "$PRODUCT" == "AC1200MG" ] ;then
		cp uboot_bin/uboot_7530_MD5_SPI_DDR2_64M_GPIO13.bin uboot.bin
		wanpid=0
		ssid=ChinaNet-
		lanip=192.168.101.1
		www_dir=www_wiair_I5
		reset=13
		sed '5a\config7530Esw WLLLL' $mu>$mu.bak
		cat $mu.bak > $mu
		config=config_7612e_telcom_usb
		kconfig=$kdir/config_telcom_usb
		product_file=product_def.h
		flash_size=8M
		gcc_define=-DWPS_REPEATER
	elif [ "$PRODUCT" = "BL-WR328" ]; then
		config=config_4+32_no_ali_BL-WR328
		www_dir=www_snblink
		gcc_define=-DWPS_NO_LED
	elif [ "$PRODUCT" = "AC886" ]; then
		gcc_define=-DWPS_NO_LED
	elif [ "$PRODUCT" = "WR4000" ]; then
		cp uboot_bin/uboot_7620_MD5_SPI_SDR_32M_GPIO2.bin uboot.bin
		gcc_define=-DWPS_NO_LED
	elif [ "$PRODUCT" = "WR308" ]; then
		cp uboot_bin/uboot_7620_MD5_SPI_SDR_32M_GPIO2.bin uboot.bin
		gcc_define=-DWPS_NO_LED
	fi
	;;
	"YOCAWIFI")
	if [ "$PRODUCT" == "1S" ]; then
		telnet_port=23
		domain=wifi.yocabox.com
		wanpid=0
		sed '5a\config6855Esw WLLLL' $mu>$mu.bak
		cat $mu.bak > $mu
		ssid=yocawifi-
		config=config_8+64_no_ali
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
		lanip=192.168.1.1
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR2_64M_GPIO1.bin uboot.bin
		cp -rf factory_wiair.bin factory.bin
		reset=1
		www_dir=www_we1726
		gcc_define=-DWPS_NOT_SUPPORT
	fi
	;;
	"EDUP")
	if [ "$PRODUCT" == "V1" ]; then
		wanpid=0
		sed '5a\config6855Esw WLLLL' $mu>$mu.bak
		cat $mu.bak > $mu
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR1_64M_GPIO1.bin uboot.bin
		cp -rf factory_wiair.bin factory.bin
		reset=1
		product_file=product_def.h
		flash_size=8M
		www_dir=www_edup
		config=config_8+64_no_ali_usb_KZL_WR507
		kconfig=$kdir/config_8+64_usb
		lanip=192.168.10.1
		ssid=EDUP_
	fi
	;;
	"HUITEFONE")
	if [ "$PRODUCT" == "8305S" ]; then
		cp -rf factory_wiair.bin factory.bin
		firmware=/dev/mtd5
		wanpid=4
		sed '5a\config6855Esw LLLLW' $mu > $mu.bak
		cat $mu.bak > $mu
		reset=1
		config=config_4+32_no_ali
		kconfig=$kdir/config_4+64
		product_file=product_4_32.h
		flash_size=4M
		www_dir=www_huitefone
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR2_64M_GPIO1.bin uboot.bin
		ssid=Huitefone_
		lanip=192.168.8.1
	fi
	;;
	"GAOKE")
	lanip=192.168.8.1
	ssid=GAOKE_
	domain=gaoke.com
	telnet_port=23
	login_pwd=gaoke
	cp -rf uboot_7620_32m_ddr1_gpio1.bin uboot.bin
	if [ "$PRODUCT" = "$VENDOR" ]; then
		PRODUCT=mt7620_4+32
		wanpid=1
		sed '5a\config6855Esw LWLLL' $mu>$mu.bak
		config=config_4+32
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke
	elif [ "$PRODUCT" == "Q307R" ]; then
		wanpid=0
		sed '5a\config6855Esw WLLLL' $mu>$mu.bak
		config=config_4+32
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke
	elif [ "$PRODUCT" == "W310" ]; then
		wanpid=0
		sed '5a\config6855Esw WLLLL' $mu>$mu.bak
		config=config_4+32
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke_fali
		gcc_define=-DNO_ALI_JS
	elif [ "$PRODUCT" == "Q363A" ]; then
		wanpid=1
		sed '5a\config6855Esw LWLLL' $mu>$mu.bak
		ssid=jiwang_
		login_pwd=admin
		domain=
		config=config_4+32_no_ali
		kconfig=$kdir/config_4+32
		product_file=product_4_32.h
		flash_size=4M
		www_dir=www_gaoke_q363a
		gcc_define="-DNO_ALI_JS -DWPS_NO_LED"
		cp -rf ./uboot_bin/uboot_7620_MD5_SPI_DDR1_32M_GPIO2.bin uboot.bin
		lanip=192.168.10.1
	elif [ "$PRODUCT" == "Q307R_V2" ]; then
		config=config_4+32_no_ali
		wanpid=0
		sed '5a\config6855Esw WLLLL' $mu>$mu.bak
		www_dir=www_gaoke_fali
	elif [ "$PRODUCT" == "Q335" ] || \
		[ "$PRODUCT" == "Q331" ]; then
		wanpid=1
		sed '5a\config6855Esw LWLLL' $mu>$mu.bak
		config=config_4+32_no_ali
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke_fali
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR1_32M_GPIO2.bin uboot.bin
		gcc_define=-DWPS_NOT_SUPPORT
	elif [ "$PRODUCT" == "W307" ]; then
		wanpid=1
		sed '5a\config6855Esw LWLLL' $mu>$mu.bak
		config=config_4+32_no_ali
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke_fali
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR1_32M_GPIO2.bin uboot.bin
	else
		wanpid=1
		sed '5a\config6855Esw LWLLL' $mu>$mu.bak
		config=config_4+32
		kconfig=$kdir/config_4+32
		www_dir=www_gaoke
	fi
	cat $mu.bak > $mu
	;;
	"WIAIR")
	cp -rf uboot_wiair.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	vendor=CY_WiFi
	VENDOR=CY_WiFi
	www_dir=www_wiair
	firmware=/dev/mtd5
	gpio_wps=
	gpio_wifi24g=
	if [ "$PRODUCT" == "I1C" ];then
		cp uboot_bin/uboot_7620_MD5_SPI_DDR1_32M_GPIO1.bin uboot.bin
		config=config_4+32
		kconfig=$kdir/config_4+32
		product_file=product_4_32.h
		flash_size=4M
	elif [ "$PRODUCT" == "I1U" ];then
		config=config_8+64_no_ali_usb
		kconfig=$kdir/config_8+64_usb
		product_file=product_def.h
		flash_size=8M
	elif [ "$PRODUCT" == "I15" ] ;then
		config=config_7612e
		kconfig=$kdir/config_7612e
		product_file=product_def.h
		flash_size=8M
		www_dir=www_wiair_en
	elif [ "$PRODUCT" == "I3" ] ;then
		config=config_7621
		kconfig=$kdir/config_7621
		product_file=product_def.h
		flash_size=8M
	elif [ "$PRODUCT" == "I1" ] ;then
		#ali_baby
		config=config_8+64
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
	else
		config=config_8+64
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
	fi
	;;
	"PHICOMM")
	cp -rf uboot_phicomm.bin uboot.bin
	cp -rf factory_phicomm.bin factory.bin
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	domain=phicomm.me
	ssid=PHICOMM_
	firmware=/dev/mtd5
	www_dir=www_phicomm_ac
	config=config_7612e
	kconfig=$kdir/config_7612e
	product_file=product_def.h
	flash_size=8M
	;;
	"HAIER")
	if [ "$PRODUCT" = "$VENDOR" ]; then
		PRODUCT=mt7620_4+32
	fi
	wanpid=4
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	cp -rf uboot_blink_32m_sdr.bin uboot.bin
	cp -rf factory_blink.bin factory.bin
	lanip=192.168.68.1
	domain=hello.wifi.haier
	ssid=Haier-WiFi-
	www_dir=www_haier
	reset=2
	;;
	"ZHIBOTONG")
	cp -rf factory_wiair.bin factory.bin
	firmware=/dev/mtd5
	wanpid=4
	sed '5a\config6855Esw LLLLW' $mu > $mu.bak
	cat $mu.bak > $mu
	reset=1
	lanip=192.168.8.1
	if [ "$PRODUCT" = "ZYJXA-1S" ]; then
		config=config_4+32_no_ali
		kconfig=$kdir/config_4+64
		product_file=product_4_32.h
		flash_size=4M
		www_dir=www_zhibotong_1s
		cp -rf uboot_DDR2_4+64.bin uboot.bin
		gcc_define=-DZHIBOTONG_NEED
		ssid=CMCC-ZYJXA-1S
	elif [ "$PRODUCT" = "RN401" ]; then
		config=config_8+64_no_ali_arp
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
		lanip=192.168.1.1
		www_dir=www_zhibotong_chuangwei
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR2_64M_GPIO1.bin uboot.bin
		ssid=SkyWifi_
		gcc_define=-DWPS_NOT_SUPPORT
	elif [ "$PRODUCT" = "ZYJXB-1H" ]; then
		config=config_8+64_no_ali_usb
		kconfig=$kdir/config_8+64_usb
		product_file=product_def.h
		flash_size=8M
		www_dir=www_zhibotong
		cp -rf uboot_wiair.bin uboot.bin
		gcc_define=-DZHIBOTONG_NEED
		ssid=CMCC-ZYJXB-1H
	elif [ "$PRODUCT" = "WE26" ]; then
		config=config_8+64_no_ali
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
		www_dir=www_zhibotong_we26
		cp -rf uboot_wiair.bin uboot.bin
		model=$PRODUCT
		ssid=WE26_
	fi
	telnet_port=23
	;;
	"DEMO")
	cp -rf uboot_wiair.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	if [ "$PRODUCT" == "D864" ];then
		www_dir=www_demo_8_64
		firmware=/dev/mtd5
		config=config_8+64
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
	else
		www_dir=www_demo_8_64
		firmware=/dev/mtd5
		config=config_4+32
		kconfig=$kdir/config_4+32
		product_file=product_4_32.h
		flash_size=4M
	fi
	;;
	"KAER")
	if [ "$PRODUCT" == "KL100" ];then
		www_dir=www_kaer_kl100
		wanpid=4
		sed '5a\config6855Esw LLLLW' $mu>$mu.bak
		cat $mu.bak > $mu
		config=config_4+32_no_ali_arp
		kconfig=$kdir/config_4+32
		product_file=product_4_32_kaer_kl100.h
		flash_size=4M
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR1_32M_GPIO1.bin uboot.bin
		cp -rf factory_blink.bin factory.bin
		lanip=192.168.1.1
		ssid=KE2.4-wireless
		gcc_define=-DWPS_NO_LED
	fi
	;;
	"MEISIQI")
	cp -rf uboot_blink_32m_sdr.bin uboot.bin
	cp -rf factory_blink.bin factory.bin
	wanpid=4
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	lanip=192.168.2.1
	ssid=MEISIQI_
	domain=wiair.cc
	www_dir=www_meisiqi
	config=config_4+32_no_ali
	kconfig=$kdir/config_4+32
	product_file=product_4_32.h
	flash_size=4M
	;;
	"ANTBANG")
	cp -rf uboot_antbang_a3.bin uboot.bin
	cp -rf factory_blink.bin factory.bin
	wanpid=0
	sed '5a\config6855Esw WLLLL' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	lanip=192.168.249.1
	ssid=antbang_
	domain=web.antbang.com
	www_dir=www_antbang
	config=config_4+32
	kconfig=$kdir/config_4+32
	if [ "$PRODUCT" = "A3C" ] || [ "$PRODUCT" = "A5C" ]; then
		config=config_4+32_no_ali
		www_dir=www_antbang_a3c
		retime=1,1,2,4,0
	elif [ "$PRODUCT" = "A3S" ]; then
		#ali_baby
		config=config_4+32
		retime=1,1,2,4,0
	elif [ "$PRODUCT" = "A3" ]; then
		gcc_define=-DWPS_NOT_SUPPORT
		retime=1,1,2,4,0
	fi
	product_file=product_4_32.h
	flash_size=4M
	;;
	"TUOSHI")
	cp -rf uboot_wiair.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	config=config_7612e
	kconfig=$kdir/config_7612e_tuoshi
	product_file=product_def.h
	flash_size=8M
	www_dir=www_wiair_en
	;;
	"KZL")
	wanpid=0
	sed '5a\config6855Esw WLLLL' $mu>$mu.bak
	cat $mu.bak > $mu
	cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR1_64M_GPIO1.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	reset=1
	product_file=product_def.h
	flash_size=8M
	www_dir=www_kzlgd
	config=config_8+64_no_ali_usb_KZL_WR507
	kconfig=$kdir/config_8+64_usb
	lanip=192.168.0.1
	ssid=KZL-WR507
	;;
	"SZXS")
	cp -rf uboot_wiair.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	sed '5a\config6855Esw LLLLW' $mu>$mu.bak
	cat $mu.bak > $mu
	reset=1
	vendor=SZXS
	VENDOR=SZXS
	www_dir=www_wiair
	firmware=/dev/mtd5
	config=config_szxs_8+64
	kconfig=$kdir/config_8+64
	product_file=product_def.h
	flash_size=8M
	;;
	"JCC")
	wanpid=4
	sed '5a\config6855Esw LLLLW' $mu > $mu.bak
	cat $mu.bak > $mu
	cp -rf uboot_wiair.bin uboot.bin
	cp -rf factory_wiair.bin factory.bin
	firmware=/dev/mtd5
	lanip=192.168.10.1
	if [ "$PRODUCT" = "CD8600" ]; then
		config=config_8+64_no_ali_usb
		kconfig=$kdir/config_8+64_usb
		product_file=product_def.h
		flash_size=8M
		www_dir=www_jcc
		reset=1
		ssid=Wireless-N+
	fi
	;;
	"WFYL")
	if [ "$PRODUCT" == "WF_1620A" ];then
		wanpid=4
		sed '5a\config6855Esw LLLLW' $mu > $mu.bak
		cat $mu.bak > $mu
		cp -rf uboot_bin/uboot_7620_MD5_SPI_DDR2_64M_GPIO1.bin uboot.bin
		cp -rf factory_wiair.bin factory.bin
		config=config_8+64_no_ali
		kconfig=$kdir/config_8+64
		product_file=product_def.h
		flash_size=8M
		www_dir=www_wf_1620a
		ssid=WFLink-1620-
		gcc_define="-DWPS_NOT_SUPPORT -DWPS_NO_LED"
	fi
	;;
	*)
	echo "does not support"
	exit -1
	;;
esac

sed $file -e "
	s/^VENDOR=.*/VENDOR=$vendor/g;
	s/^PRODUCT=.*/PRODUCT=$product/g;
	s/^DATE=.*/DATE=$date/g;
	s/^WANPID=.*/WANPID=$wanpid/g;
	s/^RESET=.*/RESET=$reset/g;
	s/^RESET_TIME=.*/RESET_TIME=$reset_time/g;
	s/^LANIP=.*/LANIP=$lanip/g;
	s/^DOMAIN=.*/DOMAIN=$domain/g;
	s/^SSID=.*/SSID=$ssid/g;
	s#^FIRMWARE=.*#FIRMWARE=$firmware#g;
	s/^TELNET_PORT=.*/TELNET_PORT=$telnet_port/g;
	s/^LOGIN_USER=.*/LOGIN_USER=$login_user/g;
	s/^LOGIN_PWD=.*/LOGIN_PWD=$login_pwd/g;
	s/^REGION=.*/REGION=$region/g;
	s/^AREGION=.*/AREGION=$aregion/g;
	s/^TIMEZONE=.*/TIMEZONE=$timezone/g;
	s/^MODEL=.*/MODEL=$model/g;
	s/^RUSER=.*/RUSER=$ruser/g;
	s/^RPWD=.*/RPWD=$rpwd/g;
	s/^WUSER=.*/WUSER=$wuser/g;
	s/^WPWD=.*/WPWD=$wpwd/g;
	s/^GPIO_POWER=.*/GPIO_POWER=$gpio_power/g;
	s/^GPIO_WAN=.*/GPIO_WAN=$gpio_wan/g;
	s/^GPIO_LAN1=.*/GPIO_LAN1=$gpio_lan1/g;
	s/^GPIO_LAN2=.*/GPIO_LAN2=$gpio_lan2/g;
	s/^GPIO_LAN3=.*/GPIO_LAN3=$gpio_lan3/g;
	s/^GPIO_LAN4=.*/GPIO_LAN4=$gpio_lan4/g;
	s/^GPIO_INTERNET=.*/GPIO_INTERNET=$gpio_internet/g;
	s/^GPIO_WIFI24G=.*/GPIO_WIFI24G=$gpio_wifi24g/g;
	s/^GPIO_WIFI5G=.*/GPIO_WIFI5G=$gpio_wifi5g/g;
	s/^GPIO_WPS=.*/GPIO_WPS=$gpio_wps/g;
	s/^GPIO_USB=.*/GPIO_USB=$gpio_usb/g;
	s/^RETIME=.*/RETIME=$retime/g;
	">$out

export BRIDGEMODE=$bridgemode
export GCC_DEFINE=$gcc_define
export FLASH_SIZE=$flash_size
PRODUCT_DIR=$kdir/include/linux/netfilter_ipv4/nf_igd

cp -rf $PRODUCT_DIR/$product_file $PRODUCT_DIR/product.h
cp -rf $config .config
cp -rf $kconfig $kdir/.config
rm -rf package/base-files/files/www
cp -rf package/web/$www_dir package/base-files/files/www
ln -s /tmp/logfile.gz package/base-files/files/www/logfile.gz
ln -s /tmp/config.tar.gz package/base-files/files/www/config.tar.gz
make V=99
svn revert $mu

