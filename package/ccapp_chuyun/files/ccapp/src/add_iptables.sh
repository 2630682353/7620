#!/bin/sh
nginx_port="8081"
kproxy_check(){
    local cc_proxy=`iptables -t nat -L|grep "1.1.1.1"`
	if [ -z "$cc_proxy" ]; then
	    ipaddr=`ifconfig  br-lan | grep 'inet addr:'| grep -v '127.0.0.1' |cut -d: -f2 | awk '{ print $1}'`
		/usr/bin/lua /etc/app_conf/ccapp/src/cy_proxy.lua -A -A $ipaddr:$nginx_port
    fi
}
num=0
while [[ num != 40 ]]
do
    ipaddr=`ifconfig  br-lan | grep 'inet addr:'| grep -v '127.0.0.1' |cut -d: -f2 | awk '{ print $1}'`
    if [ "$ipaddr" = "" ]; then
        sleep 4
        let "num+=1"
    else
	    kproxy_check
		sleep 2
		kproxy_check
		sleep 2
		echo set \$local_mac `ifconfig eth0|grep HWaddr |awk '{print $5}'|tr -d ":"`";">/etc/nginx/info.ccapp.conf
		mkdir -p /var/log/ccapp/cache
		mkdir -p /tmp/dnsmasq.d
		touch /var/log/ccapp/ccapp_probe.log
		/usr/bin/lua /etc/app_conf/ccapp/src/ccapp_domain_speedup.lua log &
        
  	exit 0      
    fi
done
/bin/sh /etc/app_conf/ccapp/src/check_ccapp_crontab.sh &
