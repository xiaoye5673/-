CROSS_COMPILE ?= arm-none-linux-gnueabi-
KERNELDIR ?= /home/linux/Desktop/linux-3.4.39
obj-m += fs6818_led.o
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
