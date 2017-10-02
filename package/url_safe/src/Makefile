ifneq ($(KERNELRELEASE),)
obj-m := safe.o
safe-objs += url_safe.o url_safe_common.o md5.o
else
PWD := $(shell pwd)

modules:clean
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) modules
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
endif
