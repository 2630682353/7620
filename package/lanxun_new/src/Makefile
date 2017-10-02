CFLAGS=-Wall -Iinclude -L./

SRC=$(wildcard *.c)
OBJS=$(SRC:.c=.o)



all:$(OBJS)
	$(CC) -o lanxun lanxun.o $(CFLAGS) $(LDFLAGS) -lccapp -lshare -lm -lpthread

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *.so 
