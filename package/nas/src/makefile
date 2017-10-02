TARGETS := nas
OBJS	:= $(patsubst %.cpp, %.o, $(wildcard *.cpp))
CROSS   := /opt/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-

#需要包含的头文件搜索路径
INCLUDE += -I../../../include   #txdevicesdk headers
INCLUDE += -I../include         #libnassdk.so headers
INCLUDE += -I./util
INCLUDE += -I./rapidjson
INCLUDE += -I./

#需要引用的库  
LIB		+= -L../nas_lib -lnassdk -ltxdevicesdk -lshare -llicence
LIB     += -lpthread  -lrt   -ldiskm

#需要的标志位
CXX	:= $(CROSS)g++
STRIP	:= $(CROSS)strip
CXXFLAGS+= -fpermissive -Wall -Wno-deprecated -Wno-conversion-null -fpermissive -Wno-format -std=c++0x 
CXXFLAGS += -g
#增加根据d文件自动推导编译的标志
CXXFLAGS    += $(INCLUDE)


#-include util/util.mk

#deps
DEPS    += $(patsubst %.o, %.d, $(OBJS))

.PHONY: all
all: $(DEPS) $(OBJS)
	$(CXX) -o $(TARGETS) $(OBJS) $(LIB)
	$(STRIP) $(TARGETS)

-include $(DEPS)

%.d: %.cpp
	@echo "make $@"
	@set -e $@ &&  rm -f $@; \
	$(CXX) -MM $(CFLAGS) $(CXXFLAGS) $< > $@; \

.PHONY:clean
clean:
	rm -rf $(TARGETS) $(OBJS) $(DEPS)

