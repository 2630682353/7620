#!/bin/bash
DIR_RCONF=`pwd`

rm -rf rconf
mkdir rconf
#cp -rf ../src/l7.tar.gz ./rconf
cp -rf $DIR_RCONF/rconf_src/tspeed.txt $DIR_RCONF/rconf/tspeed.txt
#cp -rf $DIR_RCONF/rconf_src/advert.txt $DIR_RCONF/rconf/advert.txt
#./encrypt $DIR_RCONF/rconf_src/vpndns.txt $DIR_RCONF/rconf/vpndns.txt
./encrypt $DIR_RCONF/rconf_src/white.txt $DIR_RCONF/rconf/white.txt
./encrypt $DIR_RCONF/rconf_src/domain.txt $DIR_RCONF/rconf/domain.txt
cat $DIR_RCONF/rconf_src/UA.txt| grep -v "#" | grep -v "*" | sed '/^$/d' > $DIR_RCONF/rconf_src/tmpua.txt
./encrypt $DIR_RCONF/rconf_src/tmpua.txt $DIR_RCONF/rconf/UA.txt
rm -rf $DIR_RCONF/rconf_src/tmpua.txt
cp -rf $DIR_RCONF/rconf_src/vender.txt $DIR_RCONF/rconf/vender.txt
./encrypt $DIR_RCONF/rconf_src/study_url.txt $DIR_RCONF/rconf/study_url.txt

cp -rf rconf_check $1/etc/
cp -rf rconf $1/etc/
mkdir alone
cp -rf alone $1/etc/
