#!/bin/sh 
DIR=$1
STATE=$DIR/state
export PATH=$PATH:$DIR/bin
export LD_LIBRARY_PATH=$DIR/lib

start() {
	if [ ! -d "/tmp/dnsmasq.d/" ]; then
		mkdir /tmp/dnsmasq.d/
		touch /tmp/dnsmasq.d/ccapp_domain_speedup.conf
	fi
	
	if [ ! -d "/tmp/ccapp/" ]; then
		mkdir /tmp/ccapp/
		cp -rf $DIR/lanxun_conf/ccapp_domain_cfg.conf   /tmp/ccapp/
		cp -rf $DIR/lanxun_conf/domain_speedup.conf     /tmp/ccapp/
		cp -rf $DIR/lanxun_conf/ccip.conf    			/tmp/ccapp/
		cp -rf $DIR/lanxun_conf/ccgeturl.conf    		/tmp/ccapp/
	fi
	
	if [ ! -d "/etc/app_conf/ccapp" ]; then
		mkdir /etc/app_conf
		mkdir /etc/app_conf/ccapp
	fi	
	
	if [ ! -d "/etc/app_conf/ccapp/conf/" ]; then
		ln -s    /tmp/ccapp   /etc/app_conf/ccapp/conf
	fi
	
	if [ ! -d "/tmp/nginx/" ]; then
		mkdir /tmp/nginx/
		cp -rf $DIR/lanxun_conf/logformat.ccapp_nginx.conf    /tmp/nginx/
	fi
	
	if [ ! -d "/etc/app_conf/ccapp/nginx/" ]; then
		ln -s    /tmp/nginx   /etc/app_conf/ccapp/nginx
	fi
	
	if [ ! -d "/etc/app_conf/ccapp/log/" ]; then
		ln -s    /tmp/nginx   /etc/app_conf/ccapp/log
	fi
	
	chmod 777 $DIR/bin/lanxun
	chmod 777 $DIR/lib/*
	$DIR/bin/lanxun &

	echo "1" > $STATE

}

stop()
{
	rm -rf /tmp/dnsmasq.d
	rm -rf /tmp/ccapp
	rm -rf /tmp/nginx
	sed -i '/conf-dir=\/tmp\/dnsmasq.d/'d /tmp/dnsmasq.conf
	killall dnsmasq
	killall lanxun
	
	echo "0" > $STATE
}

if [ "$2" = "start" ]; then
	stop
	start
elif [ "$2" = "stop" ]; then
	stop
else
	echo "init.sh path start/stop"
	exit -1
fi
