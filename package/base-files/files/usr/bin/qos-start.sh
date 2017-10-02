#!/bin/sh
RUNNING=/var/run/qos.run
check_run()
{
        etch2_2_flag=`tc class ls dev eth0.2 | grep prio`
        br0_flag=`tc class ls dev br-lan | grep prio`
        [ -n "$etch2_2_flag" ] && [ -n "$br0_flag" ] &&  echo "qos is running ..." > "$RUNNING" || echo "qos is disable." >  "$RUNNING"
}
start() {
        #check program
        echo "Start qos ..."
	up=`uci get qos_rule.qos.up`
	down=`uci get qos_rule.qos.down`
        qos.sh $up $down
        check_run
        #echo wire_prio=1 > /proc/net/sched/ifinfo
        return $?
}
stop() {
        echo "Stop qos ..."
        tc qdisc del dev eth0.2 root
        tc qdisc del dev br-lan root
        return $?
}
restart() {
        stop
	enable=`uci get qos_rule.qos.enable`
	if [ $enable == "0" ];then
		exit 0
	fi
        sleep 1
        start
}
case "$1" in
        start)
                start
                ;;
        stop)
                stop
                ;;
        restart)
                restart
                ;;
        *)
                echo $"Usage: $0 {start|stop|restart}"
                exit 1
                ;;
esac

