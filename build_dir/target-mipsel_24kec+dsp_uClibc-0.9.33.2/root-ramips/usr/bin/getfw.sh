#!/bin/sh
local nr=1
local maxnr=12
cd /tmp/fwdir
while [ $nr -le $maxnr ];do
	wget -q -c $1 -O $2 -T 10
	if [ $? == 0 ];then
		break;
	fi
	nr=$(($nr + 1))
done
touch /tmp/fwdir/wgetend
cd -
