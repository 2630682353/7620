#!/bin/bash

if [[ $1 = clean ]]; then
	echo "Make Clean"
	make clean
	
else
	echo "Make NasSDKDemo"
	make
	cp -r NasSDKDemo ./bin
fi
