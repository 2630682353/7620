#CC:=gcc
#CC:=/opt/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
#CC:=/work/7620/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.6-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc 
# source path
CXXFLAG= -I./include 
CXXLDFLAG=-L./lib    
# source files 
SRC_FILES:=$(wildcard *.cpp)

all:ECC
	@echo build complete

clean:
	rm  -rf *.o *.so

ECC:$(SRC_FILES)
	$(CC) $(SRC_FILES) -o liblicence.so  -shared -ldl -lssl -lcrypto -lstdc++ -fPIC $(CXXFLAG)  $(CXXLDFLAG) 
