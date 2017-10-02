#!/bin/sh
DIR=$1
STATE=$DIR/state
BDCLOUD=$DIR/etc/init.d/cloudPlugin.sh
export PLUGIN_ROOT=$DIR

stop() {
	$BDCLOUD stop
	$BDCLOUD disable
	
	echo "0" > $STATE
}

start() {
	$BDCLOUD start
	$BDCLOUD enable

	echo "1" > $STATE
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
