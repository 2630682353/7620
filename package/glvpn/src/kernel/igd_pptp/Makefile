ifneq ($(KERNELRELEASE),)
obj-m := vpn.o
auth-objs += vpn.o
else
PWD := $(shell pwd)

modules:clean
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) modules
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
endif
