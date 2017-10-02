ifneq ($(KERNELRELEASE),)
obj-m := tracert.o
tracert-objs += url_log.o
else
PWD := $(shell pwd)

modules:clean
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) modules
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
endif
