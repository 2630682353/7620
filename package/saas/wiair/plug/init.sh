#!/bin/sh
DIR=$1
STATE=$DIR/state
export PATH=$PATH:$DIR/bin
export LD_LIBRARY_PATH=$DIR/lib:$LD_LIBRARY_PATH

#20161025,14:28
#stop finish in 3s
stop() {
	killall saas

	echo "0" > $STATE
}

#start finish in 3s
start() {
	sleep 1;
	saas $DIR/etc/saas_conf &

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
