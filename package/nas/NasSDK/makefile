TARGETS := libnassdk.so
OBJS	:= $(patsubst %.cpp, %.o, $(wildcard *.cpp))

#如果您需要编译嵌入式平台下的libnassdk.so, 请设置CROSS变量
#CROSS   := arm-linux-gnueabihf-
#CROSS   := /opt/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-

#需要包含的头文件搜索路径
INCLUDE += -I./
INCLUDE += -I../include
INCLUDE += -I../../../include           # Tencent_iot_SDK/include
INCLUDE += -I./util
INCLUDE += -I./rapidjson
INCLUDE += -I./ft_processor


#需要引用的库
LIB     += -L../nas_lib -ltxdevicesdk   # Tencent_iot_SDK/lib
LIB     += -lpthread  -lrt -ldl


BUILD_TYPE = -shared

#需要的标志位
#CXX	:= $(CROSS)g++
#STRIP	:= $(CROSS)strip
CXXFLAGS+= -fpermissive -Wall -Wno-deprecated -Wno-conversion-null -fpermissive -Wno-format  -std=c++0x  -fPIC 
CXXFLAGS += -g
CXXFLAGS += $(BUILD_TYPE)

#增加根据d文件自动推导编译的标志
CXXFLAGS    += $(INCLUDE)


-include ft_processor/ft_processor.mk
-include util/util.mk

#deps
DEPS    += $(patsubst %.o, %.d, $(OBJS))

.PHONY: all
all: $(DEPS) $(OBJS)
	$(CXX) -o $(TARGETS) $(OBJS) $(LIB) $(BUILD_TYPE)
	$(STRIP) $(TARGETS)

-include $(DEPS)

%.d: %.cpp
	@echo "make $@"
	@set -e $@ &&  rm -f $@; \
	$(CXX) -MM $(CFLAGS) $(CXXFLAGS) $< > $@; \

.PHONY:clean
clean:
	rm -rf $(TARGETS) $(OBJS) $(DEPS)

