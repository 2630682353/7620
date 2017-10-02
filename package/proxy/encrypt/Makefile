CFLAGS= -Wall
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: $(OBJS)
	$(CC) -o gz_encrypt $(OBJS) --static

clean:
	rm -rf gz_encrypt *.o
