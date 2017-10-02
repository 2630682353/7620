MODEFILE=cgi.o
STRIP=$(CROSS)strip
TARGET=ioos

ALL: clean $(TARGET) setsysflag
FILE=common.o log.o server.o arraylist.o debug.o \
 json_object.o json_tokener.o json_util.o linkhash.o \
 printbuf.o connection.o protocol.o $(MODEFILE) crc32.o gzip.o csp.o

ioos: $(FILE) main.c
	$(CC) -Os -o $(TARGET)  main.c $(FILE) $(LDFLAGS) -lz -luci -lubox -lshare $(CFLAGS) $(GCC_DEFINE)
	svn info $(TOPDIR) | grep Revision >>firmware
	svn info $(TOPDIR) | grep Relative >>firmware
setsysflag: setsysflag.c
	$(CC) -Os -o setsysflag setsysflag.c $(LDFLAGS) -lshare $(CFLAGS) $(GCC_DEFINE)
%.o:%.c
	$(CC) -Os -c -o $@ $^ $(CFLAGS) $(GCC_DEFINE)
clean: 
	rm -f *.o ioos
