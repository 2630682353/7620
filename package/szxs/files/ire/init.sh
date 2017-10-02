#!/bin/sh
DIR=$1
STATE=$DIR/state
export PATH=$PATH:$DIR/bin
export LD_LIBRARY_PATH=$DIR/lib
MEMSIZE=`cat /proc/meminfo | grep MemTotal | awk '{printf $2}'`
if [ $MEMSIZE -gt 39999 ];then
	echo "64M  $MEMSIZE"
	FILESIZE=1024
	GZSIZE=1024
else
	echo "32M  $MEMSIZE"
	FILESIZE=300
	GZSIZE=500
fi

stop() {
	cloud_exchange z "$DIR/bin/xinsight $FILESIZE $GZSIZE"
	cloud_exchange z $DIR/bin/iruploader
	killall Xuploader
	
	echo "0" > $STATE
}

start() {
	chmod 777 $DIR/bin/xinsight
	cloud_exchange d "$DIR/bin/xinsight $FILESIZE $GZSIZE" $DIR
	chmod 777 $DIR/bin/iruploader
	cloud_exchange d $DIR/bin/iruploader $DIR

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
