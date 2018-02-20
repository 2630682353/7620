#!/bin/sh
append DRIVERS "mt7620"

. /lib/wifi/ralink_common.sh

prepare_mt7620() {
	prepare_ralink_wifi mt7620
}

scan_mt7620() {
	scan_ralink_wifi mt7620 mt7620
}


disable_mt7620() {
	disable_ralink_wifi mt7620
}

enable_mt7620() {
	enable_ralink_wifi mt7620 mt7620
}

detect_mt7620() {
#	detect_ralink_wifi mt7620 mt7620
	cd /sys/module/
	[ -d $module ] || return
	[ -e /etc/config/wireless ] && return
	SSID=`sed -n /SSID/p /etc/firmware | awk -F "=" '{print $2}'`
	VENDOR=`sed -n /VENDOR/p /etc/firmware | awk -F "=" '{print $2}'`
	PRODUCT=`sed -n /PRODUCT/p /etc/firmware | awk -F "=" '{print $2}'`
	case $VENDOR in
	BLINK)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z`
		;;
	GAOKE)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z`
		;;
	ZHIBOTONG)
		MAC4=
		if [ "$PRODUCT" == "RN401" ];then
			MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z`
		fi
		;;
	ANTBANG)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z`
		;;
	KZL)
		MAC4=
		;;
	*)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print ""$5""$6 }'| tr a-z A-Z`
		;;
	esac
		
         cat <<EOF
config wifi-device      mt7620
        option type     mt7620
        option vendor   ralink
        option band     2.4G
        option channel  auto
        option txpoer   100
        option auotch   2
        option enable   1

config wifi-iface
        option device   mt7620
        option ifname   ra0
        option network  lan
        option mode     ap
        option ssid	${SSID}${MAC4}
        option encryption none

config wifi-iface
        option device   mt7620
        option ifname   ra1
        option network  lan
        option mode     ap
        option ssid	${SSID}${MAC4}-Guest
        option encryption none
	option enable 0

EOF


}


