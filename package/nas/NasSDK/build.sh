#!/bin/bash

if [[ $1 = clean ]]; then
	make clean

else
	make || exit "$$?"
	cp -r libnassdk.so ../lib/
	
fi



