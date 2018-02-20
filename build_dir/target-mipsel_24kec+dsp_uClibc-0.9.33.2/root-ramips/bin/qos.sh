wan=eth0.2
lan=br-lan
up_reserve=
down_reserve=

if [ $# -lt "2" ];then
	echo "turn off qos"
	tc qdisc del dev ${wan} root
	tc qdisc del dev ${lan} root
	exit 0
fi

#function reserved_bandwidth()
{
	if [ "$1" -le 1024 ];then
		up_reserve=72
	elif [ "$1" -le 2048 ];then
		up_reserve=80
	elif [ "$1" -le 4096 ];then
		up_reserve=85
	else 
		up_reserve=90
	fi
	if [ "$2" -le 1024 ];then
		down_reserve=75
	elif [ "$2" -le 4096 ];then
		down_reserve=80
	elif [ "$2" -le 8192 ];then
		down_reserve=85
	else 
		down_reserve=90
	fi
}

#function add_qos_rule()
{
	local rate
	local ceil
	local burst
	local total
	let rate="$1"*${up_reserve}/100
	let total=${rate}

	#del current qos set
	tc qdisc del dev ${wan} root
	#add htb 
	tc qdisc add dev ${wan} root handle 1: htb 
	#class
	let burst=${total}*1024/400
	echo --${burst}
	tc class add dev ${wan} parent 1: classid 1:1 htb rate ${rate}kibit quantum 3044 burst ${burst}
	let rate=${total}/10*2
	let ceil=${total}*8/10
	let burst=${total}*1024/800
	tc class add dev ${wan} parent 1:1 classid 1:11 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 1
	tc class add dev ${wan} parent 1:1 classid 1:12 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 2
	tc class add dev ${wan} parent 1:1 classid 1:13 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 3
	let rate=${total}/10
	tc class add dev ${wan} parent 1:1 classid 1:15 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 5
	let rate=${total}/10*2
	let ceil=${total}
	tc class add dev ${wan} parent 1:1 classid 1:14 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 4
	#load sqos
	tc qdisc add dev ${wan} parent 1:11 handle 10: sqos
	tc qdisc add dev ${wan} parent 1:12 handle 20: sqos
	tc qdisc add dev ${wan} parent 1:13 handle 30: sqos
	tc qdisc add dev ${wan} parent 1:14 handle 40: sqos
	tc qdisc add dev ${wan} parent 1:15 handle 50: sqos
	#filter
	tc filter add dev ${wan}  protocol all parent 1:0 prio 1000 u32 match mark 0 0x100 flowid 1:14
	tc filter add dev ${wan}  protocol all parent 1:0 prio 1 u32 match mark 1 0xff flowid 1:11
	tc filter add dev ${wan}  protocol all parent 1:0 prio 2 u32 match mark 2 0xff flowid 1:12
	tc filter add dev ${wan}  protocol all parent 1:0 prio 3 u32 match mark 3 0xff flowid 1:13
	tc filter add dev ${wan}  protocol all parent 1:0 prio 4 u32 match mark 4 0xff flowid 1:14
	tc filter add dev ${wan}  protocol all parent 1:0 prio 5 u32 match mark 5 0xff flowid 1:15

	#lan 
	#del lan htb
	tc qdisc del dev ${lan} root
	#add htb
	tc qdisc add dev ${lan} root handle 1: htb 
	#class
	let rate="$2"*${down_reserve}/100
	let total=${rate}
	let burst=${total}*1024/400

	tc class add dev ${lan} parent 1: classid 1:1 htb rate ${total}kibit quantum 3044 burst ${burst}
	let rate=${total}/10*2
	let ceil=${total}*8/10
	let burst=${total}*1024/400
	echo ----${rate} ---${total}--${burst}
	tc class add dev ${lan} parent 1:1 classid 1:11 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 1
	tc class add dev ${lan} parent 1:1 classid 1:12 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 2
	tc class add dev ${lan} parent 1:1 classid 1:13 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 3
	let rate=${total}/10
	tc class add dev ${lan} parent 1:1 classid 1:15 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 5
	let rate=${total}/10*2
	let ceil=${total}
	tc class add dev ${lan} parent 1:1 classid 1:14 htb rate ${rate}kibit ceil ${ceil}kibit quantum 1522 burst ${burst} mpu 64 prio 4 
	#load sqos
	tc qdisc add dev ${lan} parent 1:11 handle 10: sqos
	tc qdisc add dev ${lan} parent 1:12 handle 20: sqos
	tc qdisc add dev ${lan} parent 1:13 handle 30: sqos
	tc qdisc add dev ${lan} parent 1:14 handle 40: sqos
	tc qdisc add dev ${lan} parent 1:15 handle 50: sqos
	tc qdisc add dev ${lan} parent 1:16 handle 60: sqos
	#filter
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 1000 u32 match mark 0 0x100 flowid 1:14
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 1 u32 match mark 1 0xff flowid 1:11
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 2 u32 match mark 2 0xff flowid 1:12
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 3 u32 match mark 3 0xff flowid 1:13
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 4 u32 match mark 4 0xff flowid 1:14
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 5 u32 match mark 5 0xff flowid 1:15
	tc filter add dev ${lan}  protocol ip parent 1:0 prio 6 u32 match mark 6 0xff flowid 1:16
}
exit 0

