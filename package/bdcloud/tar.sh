#!/bin/sh
D=`date '+%Y%m%d'`

cd plug
chmod 755 * -R
tar -zcvf bdcloud_7620_$D.tar.gz *
mv bdcloud_7620_$D.tar.gz ../
cd ../
