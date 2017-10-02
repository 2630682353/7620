LIBDIR=../../../package/ali/files/lib/
CFLAGS= -Wall -Iinclude -Os
LIB=-L$(LIBDIR) -lcyassl -lalinkgw -lshare -luci -lubox -ldl -lpthread -ljson-c -lm
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

TARGET=ali_cloud
all: $(OBJS)
	$(CC) -o $(TARGET) $(TARGET).o $(CFLAGS) $(LDFLAGS) $(LIB) $(GCC_DEFINE)

sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(GCC_DEFINE)
clean:
	rm -rf .depfile *.o *.so $(TARGET)
