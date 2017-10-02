#!/bin/sh 
# Copyright (C) 2006-2011 OpenWrt.org

START=99
STOP=80
NEWLINE="
"

MULTI_LAN=
NUM_CPU_CORE=

#cmd pass in to parameter
#$1 ssid number
getIntf(){
  #get all the inteface with ip addr
  oIFS=$IFS
  IFS=$NEWLINE
  UP_INTF=
  sep=
  for l in $(ifconfig -a | grep 'inet addr:\|Link encap' 2>/dev/null) ; do
    if [ "$l" != "${l##*inet addr}" ] ; then
      #find a interface with ip addr
      IFS=$oIFS
      set -- $preLine
      if [ $1 != "lo" ] ; then
        UP_INTF="${UP_INTF}${sep}$1"
        sep=,
      fi
      IFS=$NEWLINE
    fi
    preLine="$l"
  done
  IFS=$oIFS
  echo -n "$UP_INTF"
}

killAllProcess()
{
  
  proc=$1
  
  if which killall >/dev/null 2>/dev/null ; then
    killall -9 $proc
  else
    oIFS=$IFS 
    IFS=$NEWLINE
    for l in $(ps | grep -v 'PID USER' 2>/dev/null) ; do  
      IFS=$oIFS
      set -- $l 
      pid=$1 
      if [ "s$pid" != "s" ] && [ $pid != PID ] && [ $pid -ne 0 ] && [ -r /proc/$pid/cmdline ] ; then
        cmdline=$(cat /proc/$pid/cmdline)
        if echo "$cmdline" | grep $proc | grep -v cloudPlugin.sh | grep -v cloudPluginDL | grep -v grep >/dev/null 2>/dev/null ; then
          $LOGGER "kill process pid:$pid $cmdline"
          kill $pid && sleep 3 && kill -0 $pid 2>/dev/null &&  kill -9 $pid
        fi
      fi
      IFS=$NEWLINE
    done 
    IFS=$oIFS
  fi
}
start() {
  OPENWRT_VERSION=
  for f in openwrt_release openwrt_version ; do 
    [ ! -r /etc/$f ]  && continue
    if cat /etc/$f | grep -i barrier | grep -i breaker 2>/dev/null >/dev/null ; then
      OPENWRT_VERSION=14.07
      break
    elif cat /etc/$f | grep -i attitude | grep -i adjustment  2>/dev/null >/dev/null ; then
      OPENWRT_VERSION=12.09
      break
    elif cat /etc/$f | grep -i chaos | grep -i calmer  2>/dev/null >/dev/null ; then
      OPENWRT_VERSION=15.05
      break
    fi
  done
  [ "s$OPENWRT_VERSION" != "s" ] && OPENWRT_VERSION="-v $OPENWRT_VERSION"

  if [ "s$NOT_OPENWRT" == "s1" ] ; then
    #not openwrt
    ${SERVICE_EXEC} -p ${SERVICE_PID_FILE} -i ${AP_MAC_INTF} "${AP_LAN_INTF}" -r ${REPORT_INTERVAL} $GET_CFG_INTERVAL >/dev/null 2>/dev/null &
  else
    service_start ${SERVICE_EXEC} -p ${SERVICE_PID_FILE} -i ${AP_MAC_INTF} "${AP_LAN_INTF}" -r ${REPORT_INTERVAL} $GET_CFG_INTERVAL $OPENWRT_VERSION
  fi
  #start monitor process
  if [ ! -r $MONITOR_PID_FILE ] ; then
    $PLUGIN_ROOT/usr/share/cloudPlugin/cloudPluginMon.sh $SERVICE_PID_FILE $MONITOR_PID_FILE >/dev/null 2>/dev/null &
  else
    mon_pid=$(cat $MONITOR_PID_FILE 2>/dev/null)
    if [ "s$mon_pid" == "s" ]  || ! kill -0 $mon_pid ; then
      $PLUGIN_ROOT/usr/share/cloudPlugin/cloudPluginMon.sh $SERVICE_PID_FILE $MONITOR_PID_FILE >/dev/null 2>/dev/null &
    fi
  fi

}

stop() {
  mon_pid=$(cat $MONITOR_PID_FILE 2>/dev/null)
  if [ "s$mon_pid" != "s" ] ; then 
    kill -9 $mon_pid
    rm -fr $MONITOR_PID_FILE
  fi
  if [ "s$NOT_OPENWRT" == "s1" ] ; then
    killAllProcess cloudPluginMon
    killAllProcess cloudPlugin
  else
    killall -9 cloudPluginMon.sh
    service_stop ${SERVICE_EXEC}
  fi
}


#set -x 
RUN_DIR=/var/run
if [ "s$RUN_DIR" != "s" ] ; then 
  [ -d $RUN_DIR ] || mkdir -p $RUN_DIR
fi
TEMP_DIR=/tmp
#PLUGIN_ROOT=/tmp/cloudPluginRoot
NOT_OPENWRT=1
SERVICE_USE_PID=1
SERVICE_DAEMONIZE=1
SERVICE_PID_FILE=$RUN_DIR/cloudPlugin.pid
MONITOR_PID_FILE=$RUN_DIR/cloudPluginMon.pid
#SERVICE_DEBUG=1
SERVICE_EXEC=$PLUGIN_ROOT/usr/share/cloudPlugin/cloudPlugin
AP_MAC_INTF_FILE=$PLUGIN_ROOT/etc/ap_mac_interface
DEFAULT_AP_MAC_INTF=br-lan
AP_MAC_INTF=$DEFAULT_AP_MAC_INTF
DEFAULT_AP_LAN_INTF=
MULTI_LAN=
LOGGER=echo

if logger test >/dev/null 2>/dev/null ; then
  LOGGER=logger
fi

if [ "s$MULTI_LAN" = s1 ] ; then
  AP_LAN_INTF="-l $(getIntf)"
elif [ "s$DEFAULT_AP_LAN_INTF" != s ] ; then
  AP_LAN_INTF="-l $DEFAULT_AP_LAN_INTF"
fi


if [ "s$NOT_OPENWRT" == "s1" ] ; then
  action=$1
  shift 1
fi

GET_CFG_INTERVAL=

[ "s$GET_CFG_INTERVAL" != "s" ] && GET_CFG_INTERVAL="-g $GET_CFG_INTERVAL"

if [ -r ${AP_MAC_INTF_FILE} ] && [ -e /sys/class/net/$(cat ${AP_MAC_INTF_FILE}) ] ; then
  AP_MAC_INTF=$(cat ${AP_MAC_INTF_FILE})
fi

if [ "s$AP_MAC_INTF" == "s" ] ; then
    AP_MAC_INTF="br-lan"
fi

if [ ! -e /sys/class/net/$AP_MAC_INTF ] ; then
  AP_MAC_INTF="eth0"
fi
#interval to report info to cloud in terms of seconds
REPORT_INTERVAL=$2
if [ "s$REPORT_INTERVAL" == "s" ] ; then
  REPORT_INTERVAL=20
fi

if [ "s$NOT_OPENWRT" == "s1" ] ; then
  #not openwrt
  if [ "s$action" == "sstart" ] ; then
    start
  elif [ "s$action" == "sstop" ] ; then
    stop
  elif [ "s$action" == "srestart" ] ; then
    stop
    start
  fi
fi
