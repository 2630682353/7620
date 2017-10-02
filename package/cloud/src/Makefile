CFLAGS= -Wall -Iinclude -Os
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o cloud_client cloud_client.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	$(CC) -o cloud_exchange cloud_exchange.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	$(CC) -o tspeed test_speed.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	$(CC) -o tlcy tlcy.o $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox
	
sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf .depfile *.o *.so cloud_client cloud_exchange
