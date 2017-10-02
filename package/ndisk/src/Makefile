CFLAGS= -Wall -Iinclude -Os
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
DST=ndisk

all: $(OBJS)
	$(CC) -o $(DST) $(OBJS) $(CFLAGS) $(LDFLAGS) -lshare -luci -lubox -lsqlite3

sinclude .dep
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .dep

clean:
	rm -rf .dep *.o $(DST)
