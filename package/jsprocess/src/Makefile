ifneq ($(KERNELRELEASE),)
obj-m := js.o
tracert-objs += js.o
else
PWD := $(shell pwd)
LIBS = -lpthread

modules:clean jsprocess
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) modules

jsprocess: jsprocess.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.c $(LDLIBS) $(LIBS)

clean:
	-rm -f jsprocess *.elf *.gdb *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
endif