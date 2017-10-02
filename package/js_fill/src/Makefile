ifneq ($(KERNELRELEASE),)
obj-m := fill.o
fill-objs += js_fill.o
else
PWD := $(shell pwd)

modules:clean
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) modules
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
endif
