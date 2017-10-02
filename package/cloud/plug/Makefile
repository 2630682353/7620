CFLAGS=$(CROSS_CFLAGS)
LDFLAGS=$(CROSS_LDFLAGS)

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
SRC=cloud_client
DST=$(ROMFS_DIR)/bin/$(SRC)
DEPFILE=.dep
.PHONY: all clean dep distclean

all: $(DST)

$(DST): $(SRC)
	cp $^ $@
	chmod 777 $@
	$(CROSS_STRIP) $@

$(SRC): $(OBJS)
	$(CROSS_CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

sinclude $(DEPFILE)
dep:
	$(CROSS_CC) -MM $(CFLAGS) $(LDFLAGS) $(SRCS) > $(DEPFILE)

%.o: %.c
	$(CROSS_CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

clean:
	rm -rf *.o $(SRC)

distclean: clean
	rm -rf $(DEPFILE)
