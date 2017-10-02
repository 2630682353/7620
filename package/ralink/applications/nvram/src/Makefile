CFLAGS += -O2 -Wall

all: nvramd ralink_init

nvramd: nvram_daemon.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lnvram 
	ln -sf nvramd nvram_get
	ln -sf nvramd nvram_set

ralink_init: ralink_init.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lnvram 

clean:
	rm -f *.o $(EXEC)

