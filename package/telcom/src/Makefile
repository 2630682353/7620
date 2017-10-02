CFLAGS= -Iinclude -I./openssl-1.0.1j/include/ -I../../libshare/src/include/ -L./
SRC=$(wildcard *.c)
OBJS=$(SRC:.c=.o)

all:$(OBJS)
	$(CC) -o telcomc client.o cydh.o base64.o lib.o $(CFLAGS) $(LDFLAGS) -Wl,-Bstatic -lcrypto -Wl,-Bdynamic -lshare -ljansson -ldl -luci 

%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o *.so 
