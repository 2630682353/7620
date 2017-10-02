#!/bin/sh
append DRIVERS "mt7612"

. /lib/wifi/ralink_common.sh

prepare_mt7612() {
	prepare_ralink_wifi mt7612
}

scan_mt7612() {
	scan_ralink_wifi mt7612 mt76x2e
}

disable_mt7612() {
	disable_ralink_wifi mt7612
}

enable_mt7612() {
	enable_ralink_wifi mt7612 mt76x2e
}

detect_mt7612() {
	kver=`ls /lib/modules`
	insmod /lib/modules/$kver/mt76x2e.ko
#	detect_ralink_wifi mt7612 mt76x2e
	ssid=mt7612e-`ifconfig eth0 | grep HWaddr | cut -c 51- | sed 's/://g'`
	cd /sys/module/
	[ -d $module ] || return
	[ -e /etc/config/wireless ] && return
	SSID=`sed -n /SSID/p /etc/firmware | awk -F "=" '{print $2}'`
	VENDOR=`sed -n /VENDOR/p /etc/firmware | awk -F "=" '{print $2}'`
	case $VENDOR in
	BLINK)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print $4""$5""$6 }'| tr a-z A-Z`
		;;
	*)
		MAC4=`cat /sys/class/net/eth0/address | awk -F ":" '{print ""$5""$6 }'| tr a-z A-Z`
		;;
	esac
	 cat <<EOF
config wifi-device      mt7612
        option type     mt7612
        option vendor   ralink
        option band     5G
        option channel  0
        option auotch   2
        option txpoer   100
        option enable   1

config wifi-iface       rai0
        option device   mt7612
        option ifname   rai0
        option network  lan
        option mode     ap
        option ssid     ${SSID}${MAC4}-5G
        option encryption none
        option hidden   0

config wifi-iface       rai1
        option device   mt7612
        option ifname   rai1
        option network  lan
        option mode     ap
        option ssid     ${SSID}${MAC4}-5G-Guest
        option encryption none
        option enable   0

EOF

}


