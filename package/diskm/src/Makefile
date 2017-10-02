STRIP=$(CROSS)strip
TARGET=diskm
DSTLIB=libdiskm.so
CFLAGS= -fPIC -g -Wall -Iinclude
ALL: clean $(TARGET) fileserv_send filecp stopcp $(DSTLIB) disktst
FILE=comlib.o disk_manager.o exfat.o hfs.o diskm_ipc_msg.o \
 disk_triger.o libblkid-tiny.o ntfs.o sg_io.o vfat.o 
diskm: $(FILE) main.c
	$(CC) -o $(TARGET)  main.c $(FILE) $(LDFLAGS) -luci -lubox $(CFLAGS) $(GCC_DEFINE)
$(DSTLIB): $(FILE)
	$(CC) -o $(DSTLIB) -shared $^ $(CFLAGS) $(LDFLAGS) -luci -lubox $(GCC_DEFINE)
	$(STRIP) $(DSTLIB)
fileserv_send: fileserv_send.c
	$(CC) -O2 -o fileserv_send fileserv_send.c  $(LDFLAGS) $(CFLAGS) $(GCC_DEFINE)
filecp: filecp.c
	$(CC) -O2 -o filecp filecp.c  $(LDFLAGS) $(CFLAGS) $(GCC_DEFINE)
stopcp: stopcp.c
	$(CC) -O2 -o stopcp stopcp.c  $(LDFLAGS) $(CFLAGS) $(GCC_DEFINE)
disktst: dtest.c
	$(CC) -O2 -o disktst dtest.c  $(LDFLAGS) $(CFLAGS) -L ./ -ldiskm 
%.o:%.c
	$(CC) -c -o $@ $^ $(CFLAGS) $(GCC_DEFINE)
clean: 
	rm -f *.o diskm $(DSTLIB) 
