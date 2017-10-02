CFLAGS=-Wall -Werror -fPIC -Iinclude -Os

include ../../../kernel/linux/.config

SRC=$(wildcard *.c)
OBJS=$(SRC:.c=.o)
DSTLIB=libshare.so
DST=

ifeq ($(VENDOR),ANTBANG)
ifeq ($(PRODUCT),A3)
	CFLAGS+=-DGPIO_38_HIGH
endif
ifeq ($(PRODUCT),A3S)
	CFLAGS+=-DGPIO_38_HIGH
endif
endif

ifeq ($(CONFIG_RLX),y)
	DST=$(filter-out nlk_mtk_ipc.o, $(OBJS))
else
	DST=$(filter-out nlk_rtk_ipc.o, $(OBJS))
endif

all:$(DST)
	$(CC) -o $(DSTLIB) -shared $^ $(CFLAGS) $(LDFLAGS) -luci -lubox
	$(STRIP) $(DSTLIB)

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(LDFLAGS) -luci -lubox

clean:
	rm -f *.o *.so 
