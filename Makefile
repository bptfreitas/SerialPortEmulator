
KDIR ?= /lib/modules/`uname -r`/build


VIRTUALBOT_MAJOR=166

# VirtualBot Driver name
VIRTUALBOT_NAME="virtualbot"

.PHONY: all clean setup-environment modules_install

# 
# setup-environment: configures environment for module development
# For Debian systems, start by using 'apt install make binutils'
# 1) It downloads the current system kernel headers 
setup-environment:
	sudo apt install linux-headers-`uname -r`

all: 
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

modules_install:
	sudo $(MAKE) -C $(KDIR) M=$$PWD modules_install

install:
	sudo mknod /dev/virtualbot c 166 0
	sudo sysctl -w kernel.printk=7
	sudo chmod 777 /dev/virtualbot
	sysctl kernel.printk

uninstall:
	sudo rm /dev/virtualbot


