#!/bin/sh

restore7530Esw()
{
	echo "restore MT7530 ESW to dump switch mode"
	#port matrix mode
	switch_7530 reg w 2004 ff0000 #port0
	switch_7530 reg w 2104 ff0000 #port1
	switch_7530 reg w 2204 ff0000 #port2
	switch_7530 reg w 2304 ff0000 #port3
	switch_7530 reg w 2404 ff0000 #port4
	switch_7530 reg w 2504 ff0000 #port5
	switch_7530 reg w 2604 ff0000 #port6

	#LAN/WAN ports as transparent mode
	switch_7530 reg w 2010 810000c0 #port0
	switch_7530 reg w 2110 810000c0 #port1
	switch_7530 reg w 2210 810000c0 #port2
	switch_7530 reg w 2310 810000c0 #port3	
	switch_7530 reg w 2410 810000c0 #port4
	switch_7530 reg w 2510 810000c0 #port5
	switch_7530 reg w 2610 810000c0 #port6
	
	#clear mac table if vlan configuration changed
	switch_7530 clear
	switch_7530 vlan clear
}

config7530Esw()
{
	#init 7620

	switch reg w 2510 81000000 #port5
	#set CPU/P7 port as user port
	switch reg w 2610 81000000 #port6
	switch reg w 2710 81000000 #port7

	switch reg w 2504 20ff0003 #port5
	switch reg w 2604 20ff0003 #port6, Egress VLAN Tag Attribution=tagged
	switch reg w 2704 20ff0003 #port7, Egress VLAN Tag Attribution=tagged

	switch vlan set 0 1 00000111
	switch vlan set 1 2 00000111

	switch clear

	#LAN/WAN ports as security mode
	switch_7530 reg w 2004 ff0003 #port0
	switch_7530 reg w 2104 ff0003 #port1
	switch_7530 reg w 2204 ff0003 #port2
	switch_7530 reg w 2304 ff0003 #port3
	switch_7530 reg w 2404 ff0003 #port4
	switch_7530 reg w 2504 ff0003 #port5
	#LAN/WAN ports as transparent port
	switch_7530 reg w 2010 810000c0 #port0
	switch_7530 reg w 2110 810000c0 #port1
	switch_7530 reg w 2210 810000c0 #port2
	switch_7530 reg w 2310 810000c0 #port3	
	switch_7530 reg w 2410 810000c0 #port4
	switch_7530 reg w 2510 810000c0 #port5
	#set CPU/P7 port as user port
	switch_7530 reg w 2610 81000000 #port6
	switch_7530 reg w 2710 81000000 #port7

	switch_7530 reg w 2604 20ff0003 #port6, Egress VLAN Tag Attribution=tagged
	switch_7530 reg w 2704 20ff0003 #port7, Egress VLAN Tag Attribution=tagged
	if [ "$CONFIG_RAETH_SPECIAL_TAG" == "y" ]; then
	    echo "Special Tag Enabled"
		switch_7530 reg w 2610 81000020 #port6
		#VLAN member port
		switch_7530 vlan set  0 1 10000010
		switch_7530 vlan set  0 2 01000010
		switch_7530 vlan set  0 3 00100010
		switch_7530 vlan set  0 4 00010010
		switch_7530 vlan set  0 5 00001010
		switch_7530 vlan set  0 6 00000110
		switch_7530 vlan set  0 7 00000110
	else
	    echo "Special Tag Disabled"
		switch_7530 reg w 2610 81000000 #port6
	fi

	if [ "$1" = "LLLLW" ]; then
		if [ "$CONFIG_RAETH_SPECIAL_TAG" == "y" ]; then
		#set PVID
		switch_7530 reg w 2014 10007 #port0
		switch_7530 reg w 2114 10007 #port1
		switch_7530 reg w 2214 10007 #port2
		switch_7530 reg w 2314 10007 #port3
		switch_7530 reg w 2414 10008 #port4
		switch_7530 reg w 2514 10007 #port5
		#VLAN member port
		switch_7530 vlan set 0 1 10000010
		switch_7530 vlan set 0 2 01000010
		switch_7530 vlan set 0 3 00100010
		switch_7530 vlan set 0 4 00010010
		switch_7530 vlan set 0 5 00001010
		switch_7530 vlan set 0 6 00000110
		switch_7530 vlan set 0 7 11110110
		switch_7530 vlan set 0 8 00001010
		else
		#set PVID
		switch_7530 reg w 2014 10001 #port0
		switch_7530 reg w 2114 10001 #port1
		switch_7530 reg w 2214 10001 #port2
		switch_7530 reg w 2314 10001 #port3
		switch_7530 reg w 2414 10002 #port4
		switch_7530 reg w 2514 10001 #port5
		#VLAN member port
		switch_7530 vlan set  1 11110110
		switch_7530 vlan set  2 00001010
		fi
	elif [ "$1" = "WLLLL" ]; then
		if [ "$CONFIG_RAETH_SPECIAL_TAG" == "y" ]; then
		#set PVID
		switch_7530 reg w 2014 10008 #port0
		switch_7530 reg w 2114 10007 #port1
		switch_7530 reg w 2214 10007 #port2
		switch_7530 reg w 2314 10007 #port3
		switch_7530 reg w 2414 10007 #port4
		switch_7530 reg w 2514 10007 #port5
		#VLAN member port
		switch_7530 vlan set  0 5 10000010
		switch_7530 vlan set  0 1 01000010
		switch_7530 vlan set  0 2 00100010
		switch_7530 vlan set  0 3 00010010
		switch_7530 vlan set  0 4 00001010
		switch_7530 vlan set  0 6 00000110
		switch_7530 vlan set  0 7 01111110
		switch_7530 vlan set  0 8 10000010
		else
		#set PVID
		switch_7530 reg w 2014 10002 #port0
		switch_7530 reg w 2114 10001 #port1
		switch_7530 reg w 2214 10001 #port2
		switch_7530 reg w 2314 10001 #port3
		switch_7530 reg w 2414 10001 #port4
		switch_7530 reg w 2514 10001 #port5
		#VLAN member port
		switch_7530 vlan set  1 0111111
		switch_7530 vlan set  2 1000001
		fi
	elif [ "$1" = "W1234" ]; then
		echo "W1234"
		#set PVID
		switch_7530 reg w 2014 10005 #port0
		switch_7530 reg w 2114 10001 #port1
		switch_7530 reg w 2214 10002 #port2
		switch_7530 reg w 2314 10003 #port3
		switch_7530 reg w 2414 10004 #port4
		switch_7530 reg w 2514 10006 #port5
		#VLAN member port
		switch_7530 vlan set  0 5 10000010
		switch_7530 vlan set  0 1 01000010
		switch_7530 vlan set  0 2 00100010
		switch_7530 vlan set  0 3 00010010
		switch_7530 vlan set  0 4 00001010
		switch_7530 vlan set  0 6 00000110
	   
	elif [ "$1" = "12345" ]; then
		echo "12345"
		#set PVID
		switch_7530 reg w 2014 10001 #port0
		switch_7530 reg w 2114 10002 #port1
		switch_7530 reg w 2214 10003 #port2
		switch_7530 reg w 2314 10004 #port3
		switch_7530 reg w 2414 10005 #port4
		switch_7530 reg w 2514 10006 #port5
		#VLAN member port
		switch_7530 vlan set  0 1 10000010
		switch_7530 vlan set  0 2 01000010
		switch_7530 vlan set  0 3 00100010
		switch_7530 vlan set  0 4 00010010
		switch_7530 vlan set  0 5 00001010
		switch_7530 vlan set  0 6 00000110
	elif [ "$1" = "GW" ]; then
		echo "GW"
		#set PVID
		switch_7530 reg w 2014 10001 #port0
		switch_7530 reg w 2114 10001 #port1
		switch_7530 reg w 2214 10001 #port2
		switch_7530 reg w 2314 10001 #port3
		switch_7530 reg w 2414 10001 #port4
		switch_7530 reg w 2514 10002 #port5
		#VLAN member port
		switch_7530 vlan set  0 1 11111010
		switch_7530 vlan set  0 2 00000110
	fi

	#clear mac table if vlan configuration changed
	switch_7530 clear
}

restore6855Esw()
{
    echo "restore GSW to dump switch mode"
    #port matrix mode
    switch reg w 2004 ff0000 #port0
    switch reg w 2104 ff0000 #port1
    switch reg w 2204 ff0000 #port2
    switch reg w 2304 ff0000 #port3
    switch reg w 2404 ff0000 #port4
    switch reg w 2504 ff0000 #port5
    switch reg w 2604 ff0000 #port6
    switch reg w 2704 ff0000 #port7

    #LAN/WAN ports as transparent mode
    switch reg w 2010 810000c0 #port0
    switch reg w 2110 810000c0 #port1
    switch reg w 2210 810000c0 #port2
    switch reg w 2310 810000c0 #port3   
    switch reg w 2410 810000c0 #port4
    switch reg w 2510 810000c0 #port5
    switch reg w 2610 810000c0 #port6
    switch reg w 2710 810000c0 #port7
    
    #clear mac table if vlan configuration changed
    switch clear
}

config6855Esw()
{
	#LAN/WAN ports as security mode
	switch reg w 2004 ff0003 #port0
	switch reg w 2104 ff0003 #port1
	switch reg w 2204 ff0003 #port2
	switch reg w 2304 ff0003 #port3
	switch reg w 2404 ff0003 #port4
	switch reg w 2504 ff0003 #port5
	#LAN/WAN ports as transparent port
	switch reg w 2010 810000c0 #port0
	switch reg w 2110 810000c0 #port1
	switch reg w 2210 810000c0 #port2
	switch reg w 2310 810000c0 #port3	
	switch reg w 2410 810000c0 #port4
	switch reg w 2510 810000c0 #port5
	#set CPU/P7 port as user port
	switch reg w 2610 81000000 #port6
	switch reg w 2710 81000000 #port7

	switch reg w 2604 20ff0003 #port6, Egress VLAN Tag Attribution=tagged
	switch reg w 2704 20ff0003 #port7, Egress VLAN Tag Attribution=tagged
	if [ "$CONFIG_RAETH_SPECIAL_TAG" == "y" ]; then
	    echo "Special Tag Enabled"
		switch reg w 2610 81000020 #port6

	else
	    echo "Special Tag Disabled"
		switch reg w 2610 81000000 #port6
	fi

	if [ "$1" = "LLLLW" ]; then
		switch reg w 2014 10001 #port0
		switch reg w 2114 10001 #port1
		switch reg w 2214 10001 #port2
		switch reg w 2314 10001 #port3
		switch reg w 2414 10002 #port4
		switch reg w 2514 10001 #port5
		#VLAN member port
		switch vlan set 0 1 11110111
		switch vlan set 1 2 00001011
	elif [ "$1" = "WLLLL" ]; then
		#set PVID
		switch reg w 2014 10002 #port0
		switch reg w 2114 10001 #port1
		switch reg w 2214 10001 #port2
		switch reg w 2314 10001 #port3
		switch reg w 2414 10001 #port4
		switch reg w 2514 10001 #port5
		#VLAN member port
		switch vlan set 0 1 01111111
		switch vlan set 1 2 10000011
	elif [ "$1" = "LWLLL" ]; then
		echo "LWLLL"
		switch reg w 2014 10001 #port0
		switch reg w 2114 10002 #port1
		switch reg w 2214 10001 #port2
		switch reg w 2314 10001 #port3
		switch reg w 2414 10001 #port4
		switch reg w 2514 10001 #port5
		#VLAN member port
		switch vlan set 0 1 10111111
		switch vlan set 1 2 01000011
	fi

	#clear mac table if vlan configuration changed
	switch clear
}

# work for 7620a and 7621
setup_switch()
{
	config6855Esw LLLLW
}

reset_switch()
{
    restore6855Esw
}
