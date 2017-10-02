#!/bin/sh
append DRIVERS "mt7610"

. /lib/wifi/ralink_common.sh

prepare_mt7610() {
	prepare_ralink_wifi mt7610
}

scan_mt7610() {
	scan_ralink_wifi mt7610 mt7610
}

disable_mt7610() {
	disable_ralink_wifi mt7610
}

enable_mt7610() {
	enable_ralink_wifi mt7610 mt7610
}

detect_mt7610() {
#	detect_ralink_wifi mt7610 mt7610
	cd /sys/module
	[ -d $module ] || return
        [ -e /etc/config/wireless ] && return
         cat <<EOF
config wifi-device      mt7610
        option type     mt7610
        option vendor   ralink
        option band     5G
        option channel  0
	option autoch   2

config wifi-iface
        option device   mt7610
        option ifname   rai0
        option network  lan
        option mode     ap
        option ssid 	CYWiFi-"$MACAND4"
        option encryption psk2
        option key      12345678

EOF


}


