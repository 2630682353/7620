#!/bin/sh
DIR=$1
STATE=$DIR/state
export PATH=$PATH:$DIR/bin
export LD_LIBRARY_PATH=$DIR/lib

#stop finish in 3s
stop() {


	echo "0" > $STATE
}

#start finish in 3s
start() {


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
