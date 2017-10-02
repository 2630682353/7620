EXEC=resetd  
SRC=main.c resetd.c wps.c

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS) -lshare $(GCC_DEFINE)

%.o: %.c
	$(CC) $(CFLAGS) $(GCC_DEFINE) -c -o $@ $<

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

