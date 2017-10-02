CFLAGS= -Wall -Iinclude -Os
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o saas $(OBJS) $(CFLAGS) -lshare

sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf .depfile *.o *.so saas
