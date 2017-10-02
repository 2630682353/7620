#!/bin/sh

oIFS=$IFS
USE_DMESG=1
NEWLINE="
"
WIFI_INTERFACES=''
IFS=$NEWLINE
for l in $(iwconfig 2>/dev/null | grep 'ESSID:\|Access Point:' 2>/dev/null ) ; do
  if echo "$l" | grep 'ESSID:' 2>/dev/null >/dev/null ; then
    #wlan3     IEEE 802.11ng  ESSID:"BHU3"  
    IFS=$oIFS
    set -- $l
    INTF=$1
  else
    bssid=${l#*Access Point: }
    bssid=${bssid%% *}
    if [ "s$bssid" != "s" ] && [ $bssid != "Not-Associated" ] ; then
      add=0
      if [ s$WIFI_INTERFACES = s ] || [ ${WIFI_INTERFACES#*$INTF} != $WIFI_INTERFACES ] ; then
        add=1
      fi
      if [ $add -eq 1 ] ; then
        INTF_LIST="$INTF_LIST $INTF"
        eval "BSSID_$INTF=$bssid"
      fi
    fi
  fi
  IFS=$NEWLINE
done
IFS=$oIFS

WL_LIST_FILE=/proc/wl_list

for intf in $INTF_LIST ; do
  if [ "s$USE_DMESG" = s1 ] ; then
    dmesg -c >/dev/null 2>/dev/null
  fi
  iwpriv $intf show stainfo 2>/dev/null >/dev/null
  if [ "s$USE_DMESG" = s1 ] ; then
    oIFS=$IFS
    IFS=$NEWLINE
    for l in $(dmesg | grep -E '([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}' 2>/dev/null) ; do
      #[ 1727.150000] 34:02:86:10:7C:C3  1   0   0   1   3       -47    -38    0      HTMIX     20M   15    0     0     300    130    2034      , 2056, 1%
      l=${l#*]}
      IFS=$oIFS
      set -- $l
      # cat /proc/wl_list 
      # 34:02:86:10:7C:C3 1 0 3 20M 15 -31 -30 ra1
      eval echo "mac=$1 bssid=\$BSSID_$intf"
      IFS=$NEWLINE
    done
    IFS=$oIFS

  elif [ -r $WL_LIST_FILE ] ; then
    oIFS=$IFS
    IFS=$NEWLINE
    for l in $(cat $WL_LIST_FILE) ; do
      IFS=$oIFS
      set -- $l
      # cat /proc/wl_list 
      # 34:02:86:10:7C:C3 1 0 3 20M 15 -31 -30 ra1
      [ "$9" = $intf ] && eval echo "mac=$1 bssid=\$BSSID_$intf"
      IFS=$NEWLINE
    done
    IFS=$oIFS
  fi
done
