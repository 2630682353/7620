#!/bin/bash

export PATH=/opt/mtk7620/buildroot-gcc4.6/bin:$PATH
export STAGING_DIR=./stagdir
make clean
make
