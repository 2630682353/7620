#!/bin/sh
D=`date '+%Y%m%d'`

cd szxs
tar -zcvf szxs_7620_$D.tar.gz *
mv szxs_7620_$D.tar.gz ../

cd ../ire
tar -zcvf ire_7620_$D.tar.gz *
mv ire_7620_$D.tar.gz ../
cd ..
