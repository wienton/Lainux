# Makefile for Lainux Experimental Device Driver

obj-m += lainux_driver.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod lainux_driver.ko

uninstall:
	sudo rmmod lainux_driver

test:
	echo "Testing Lainux driver..."
	sudo dmesg | grep -i lainux
	cat /proc/lainux
	sudo dd if=/dev/urandom of=/dev/lainux bs=1 count=100
	sudo cat /dev/lainux

.PHONY: all clean install uninstall test