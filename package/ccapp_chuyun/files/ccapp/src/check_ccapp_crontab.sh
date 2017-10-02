#!/bin/sh

dnslog_heart_beat="/usr/bin/lua /etc/app_conf/ccapp/src/heart_beat.lua"

pdnsd_pro_name="pdnsd"
pdnsd_pro_start="/usr/sbin/pdnsd -c /etc/app_conf/ccapp/conf/pdnsd_ccapp.conf --daemon"
router_type="chuyun"
logrorate_pro_start="/usr/sbin/logrotate /etc/app_conf/ccapp/conf/nginxlog.conf"
nginx_port="8081"
ipaddr="127.0.0.1"
app_name="ccapp"
app_conf_root="/etc/app_conf/"
app_conf_dir=${app_conf_root}${app_name}

heart_beat(){
     $dnslog_heart_beat
}

pdnsd_check(){
	pid=`ps|grep -v grep|grep "$pdnsd_pro_name"|awk '{print $1}'`
	if [ ! -n "$pid" ]; then
		echo "start pdnsd"
		$pdnsd_pro_start
	fi
}

check_tmp_doc(){
	if [ ! -d /tmp/dnsmasq.d  ]; then
		mkdir /tmp/dnsmasq.d 		
	fi	
}
check_nginx(){
	local tempstr=`ps|grep nginx|grep -v grep|awk '{print $1}'|tr -d "\n"`
    [ -d /var/log/nginx ] || {
      mkdir /var/log/nginx
    }
    [ -f /var/log/nginx/error.log ] || { 
        touch /var/log/nginx/error.log
    }
    [ -d /var/lib/ ] || { 
        mkdir /var/lib/
    }
    [ -d /var/lib/nginx ] || { 
        mkdir /var/lib/nginx/
    }
	[ -z "$tempstr" ] && {
		nginx
	}
}
check_dnsmasq(){
	## check ccapp dns conf
	[  -f /tmp/dnsmasq.d/ccapp_dnsmasq.conf ] || {
		 lua  ${app_conf_dir}/src/ccapp_update_dnscfg.lua  ${app_conf_dir}/system.cfg 
	}
	## check dnsmasq process
	##local tempstr=`ps|grep dnsmasq|grep -v grep|awk '{print $1}'|tr -d "\n"`
	##if [[ -z "$tempstr" ]] ; then 
	##	/usr/sbin/dnsmasq -C /tmp/dnsmasq.conf -8 /tmp/dnsmasq_log -q &
	##fi

}
kproxy_check(){
    local cc_proxy=`iptables -t nat -L|grep "1.1.1.1"`
    if [ -z "$cc_proxy" ]; then
    	ipaddr=`ifconfig  br-lan | grep 'inet addr:'| grep -v '127.0.0.1' |cut -d: -f2 | awk '{ print $1}'`
		/usr/bin/lua /etc/app_conf/ccapp/src/cy_proxy.lua -A -A $ipaddr:$nginx_port
    fi
}
check(){
#    echo `date`_check>>/root/check.log
	kproxy_check
	check_tmp_doc
	check_dnsmasq
	pdnsd_check
	check_nginx
	heart_beat
}

sleep_time=1200
while :
do
	check
	sleep $sleep_time	
done
