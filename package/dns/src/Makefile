CFLAGS= -Wall -Iinclude
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o igd_dns *.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	$(STRIP) igd_dns

sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

clean:
	rm -rf .depfile *.o *.so igd_dns
