
KDIR ?= /lib/modules/`uname -r`/build


VIRTUALBOT_MAJOR=166

# VirtualBot Driver name
VIRTUALBOT_NAME="virtualbot"

.PHONY: all clean setup_dev_environment modules_install set_debug install modules_install

# setup-environment: configures environment for module development
# For Debian systems, start by using 'apt install make binutils'
# 1) It downloads the current system kernel headers 
setup_dev_environment:
	sudo apt install linux-headers-`uname -r`

all: 
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

modules_install:
	sudo $(MAKE) -C $(KDIR) M=$$PWD modules_install

set_debug:
	sudo sysctl -w kernel.printk=7
	sudo sysctl kernel.printk

check_device_logs:
	grep 'virtualbot' /var/log/kern.log

install:
	sudo insmod /lib/modules/`uname -r`/extra/carrinho.ko
	sudo mknod /dev/virtualbot c `egrep 'virtualbot' /proc/devices | cut -d' ' -f1` 0
	sudo chmod 777 /dev/virtualbot

uninstall:
	sudo rmmod /lib/modules/`uname -r`/extra/carrinho.ko
	sudo rm -f /dev/virtualbot


