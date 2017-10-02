#!/bin/sh
append DRIVERS "mt7603"

. /lib/wifi/ralink_common.sh

prepare_mt7603() {
	prepare_ralink_wifi mt7603
}

scan_mt7603() {
	scan_ralink_wifi mt7603 mt7603e
}

disable_mt7603() {
	disable_ralink_wifi mt7603e
}

enable_mt7603() {
	enable_ralink_wifi mt7603 mt7603e
}

detect_mt7603() {
#	detect_ralink_wifi mt7603e mt7603e
	cd /sys/module/
	[ -d $module ] || return
        [ -e /etc/config/wireless ] && return
         cat <<EOF
config wifi-device      mt7603
        option type     mt7603
        option vendor   ralink
        option band     2.4G
        option channel  1

config wifi-iface
        option device   mt7603
        option ifname   ra0
        option network  lan
        option mode     ap
        option ssid OpenWrt-mt7603
        option encryption psk2
        option key      12345678

EOF


}


