#!/bin/sh
sleep_cnt=1
app_name="ccapp"
app_conf_root="/etc/app_conf/"
app_conf_dir=${app_conf_root}${app_name}"/src/"
to_ftp_file="ccapp_access.log"
to_ftp_dir="/var/log/nginx/"
to_probe_file="ccapp_probe.log"
to_probe_dir="/var/log/ccapp/"
maxsize=200
probesize=20

local tempstr=`ps|grep check_ccapp_crontab|grep -v grep|awk '{print $1}'|tr -d "\n"`
if [[ -z "$tempstr" ]] ; then 
    sh ${app_conf_dir}check_ccapp_crontab.sh &
fi

fsize=`du /var/log/nginx/ccapp_access.log | awk '{print $1}'`
if [ $maxsize -gt $fsize ];then
	fsize=0
fi

psize=`du /var/log/ccapp/ccapp_probe.log | awk '{print $1}'`
if [ $probesize -gt $psize ];then
	psize=0
fi
if [ $fsize -gt 0 ]; then
    mv /var/log/nginx/ccapp_access.log /var/log/nginx/ccapp_access.log.1
    [ ! -f /var/run/nginx.pid ] || kill -USR1 `cat /var/run/nginx.pid`
    gzip /var/log/nginx/ccapp_access.log.1

    while true
    do
      if [ -f ${to_ftp_dir}${to_ftp_file}.1.gz ]; then
         to_ftp_file=${to_ftp_file}.1.gz
         size=`du ${to_ftp_dir}${to_ftp_file}|awk '{print $1}'`
         hw_addr=`ifconfig br-lan|grep HWaddr |awk '{print $5}'|tr -d ":"`
         timestamp=`date +"%s"`
         if [ $size -gt 0 ]; then
             mv ${to_ftp_dir}${to_ftp_file} ${to_ftp_dir}access_log_${hw_addr}_$timestamp.log.gz
             lua ${app_conf_dir}ccapp_reload_cfg.lua ${to_ftp_dir} ${hw_addr} ${timestamp} "nginx_log_ftp"
             sleep 1
             rm -f ${to_ftp_dir}access_log_${hw_addr}_$timestamp.log.gz
         fi
         break
      else
         sleep 1
         sleep_cnt=`expr $sleep_cnt + 1`
         if [ $sleep_cnt -gt 3 ];then
            break
         fi
      fi      
    done
fi

if [ $psize -gt 0 ]; then
    sleep_cnt=1
    mv /var/log/ccapp/ccapp_probe.log /var/log/ccapp/ccapp_probe.log.1
	touch /var/log/ccapp/ccapp_probe.log
    gzip /var/log/ccapp/ccapp_probe.log.1

    while true
    do
      if [ -f ${to_probe_dir}${to_probe_file}.1.gz ]; then
         to_probe_file=${to_probe_file}.1.gz
         size=`du ${to_probe_dir}${to_probe_file}|awk '{print $1}'`
         hw_addr=`ifconfig br-lan|grep HWaddr |awk '{print $5}'|tr -d ":"`
         timestamp=`date +"%s"`
         if [ $size -gt 0 ]; then
             mv ${to_probe_dir}${to_probe_file} ${to_probe_dir}probe_log_${hw_addr}_$timestamp.log.gz
             lua ${app_conf_dir}ccapp_reload_cfg.lua ${to_probe_dir} ${hw_addr} ${timestamp} "probe_log_ftp"
             sleep 1
             rm -f ${to_probe_dir}probe_log_${hw_addr}_$timestamp.log.gz
         fi
         break
      else
         sleep 1
         sleep_cnt=`expr $sleep_cnt + 1`
         if [ $sleep_cnt -gt 3 ];then
            break
         fi
      fi      
    done
fi
