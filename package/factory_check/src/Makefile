CFLAGS= -Wall -Iinclude -g
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o factory_check factory_check.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	
sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

clean:
	rm -rf .depfile *.o *.so 

