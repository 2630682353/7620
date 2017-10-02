#!/bin/sh
while [ 1 ]
do
        snStat=`wget http://127.0.0.1:6221 2>&1 |grep response|wc -l`
        echo $snStat
        if [[ $snStat == '0' ]] ;then /bin/sn_blink&
        else echo 'hello'
        fi
        sleep 3
done
