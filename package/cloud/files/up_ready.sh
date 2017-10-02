#!/bin/sh
rm -rf /tmp/ioos/firmware.bin
cloud_exchange z ali_cloud &
rmmod l7
rmmod safe
echo 3 > /proc/sys/vm/drop_caches
