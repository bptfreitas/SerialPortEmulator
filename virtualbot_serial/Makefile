
KDIR ?= /lib/modules/`uname -r`/build

# VirtualBot Driver name
VIRTUALBOT_NAME="virtualbot"

# VirtualBot device name
VIRTUALBOT_DEVICE=/dev/virtualbot

# Real Arduino device
# VIRTUALBOT_DEVICE=/dev/ttyACM0

.PHONY: all clean setup_dev_environment modules_install set_debug install modules_install test01

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
	
test01:
	python3 ./javython.py request $(VIRTUALBOT_DEVICE) fffe0bgetPercepts
# Javino header: fffeXY , where XY is the message length in hex
# TODO: read tem que retornar ...
# fffe49status(stopped);obstLeft(60);obstFront(49);obstRight(28);lightSensor(yes)

check_device_logs:
	grep 'virtualbot' /var/log/kern.log

install:
	sudo insmod /lib/modules/`uname -r`/extra/virtualbot.ko
	sudo mknod /dev/virtualbot c 4 200
	sudo chmod 777 /dev/virtualbot

uninstall:
	sudo rmmod /lib/modules/`uname -r`/extra/virtualbot.ko
	sudo rm -f /dev/virtualbot
	
	


