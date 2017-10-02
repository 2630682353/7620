#!/bin/sh
rm -rf $1

echo "ps" >> $1
ps >> $1 2>&1
echo "#############################################" >> $1
echo "ifconfig -a" >> $1
ifconfig -a >> $1 2>&1
echo "#############################################" >> $1
echo "route" >> $1
route >> $1 2>&1
echo "#############################################" >> $1
echo "free" >> $1
free >> $1 2>&1
echo "#############################################" >> $1
echo "lsmod" >> $1
lsmod >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/slabinfo" >> $1
cat /proc/slabinfo >> $1 2>&1
echo "#############################################" >> $1
echo "cat /etc/firmware" >> $1
cat /etc/firmware >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/igd/stat" >> $1
cat /proc/igd/stat >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/igd/filter" >> $1
cat /proc/igd/filter >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/igd/host" >> $1
cat /proc/igd/host >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/igd/url_match" >> $1
cat /proc/igd/url_match >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/net/sched/host" >> $1
cat /proc/net/sched/host >> $1 2>&1
echo "#############################################" >> $1
echo "cat /proc/net/sched/ifinfo" >> $1
cat /proc/net/sched/ifinfo >> $1 2>&1
echo "#############################################" >> $1

echo "df -h" >> $1
df -h >> $1 2>&1
echo "#############################################" >> $1

echo "du -h /tmp " >> $1
du -h /tmp/ >> $1 2>&1
echo "#############################################" >> $1

