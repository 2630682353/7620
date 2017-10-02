#!/bin/sh
append DRIVERS "mt7602"

. /lib/wifi/ralink_common.sh

prepare_mt7602() {
	prepare_ralink_wifi mt7602
}

scan_mt7602() {
	scan_ralink_wifi mt7602 mt76x2e
}

disable_mt7602() {
	disable_ralink_wifi mt7602
}

enable_mt7602() {
	enable_ralink_wifi mt7602 mt76x2e
}

detect_mt7602() {
#	detect_ralink_wifi mt7602 mt76x2e
	cd  /sys/module/
	[ -d $module ] || return
        [ -e /etc/config/wireless ] && return
         cat <<EOF
config wifi-device      mt7602
        option type     mt7602
        option vendor   ralink
        option band     2.4G
        option channel  0
        option autoch   2

config wifi-iface
        option device   mt7602
        option ifname   ra0
        option network  lan
        option mode     ap
        option ssid OpenWrt-mt7602
        option encryption psk2
        option key      12345678

EOF

}


