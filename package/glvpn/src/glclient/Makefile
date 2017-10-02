CFLAGS= -Wall -Iinclude -g
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o glclient gl_test.c -L. -lgetuisdk -lgetuiprotocol -lpthread -lshare

sinclude .depfile
dep:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depfile

clean:
	rm -rf .depfile *.o glclient
