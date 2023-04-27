
KDIR ?= /lib/modules/`uname -r`/build

MODULES_DEP = /lib/modules/$(shell uname -r)/modules.dep

# VirtualBot Driver name
VIRTUALBOT_NAME="virtualbot"

# VirtualBot device name
VIRTUALBOT_DEVICE=/dev/virtualbot

# Real Arduino device
# VIRTUALBOT_DEVICE=/dev/ttyACM0

.PHONY: all clean setup_dev_environment modules_install set_debug install modules_install tests

# setup-environment: configures environment for module development
# For Debian systems, start by using 'apt install make binutils'
# 1) It downloads the current system kernel headers 
setup_dev_environment:
	sudo apt install linux-headers-`uname -r` python3-serial make gcc binutils

all: 
	$(MAKE) -C $(KDIR) M=$$PWD modules

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

modules_install:
	sudo $(MAKE) -C $(KDIR) \
		M=$$PWD \
		modules_install 
	sudo depmod -A

set_debug:
	sudo dmesg -C
	sudo sysctl -w kernel.printk=7
	sudo sysctl kernel.printk
	
check_device_logs:
	sudo dmesg

install:
	sudo modprobe virtualbot
	sudo chmod a+rw /dev/ttyVB0
	sudo chmod a+rw /dev/ttyVB0
	sudo chmod a+rw /dev/ttyVB-Comm0
	sudo chmod a+rw /dev/ttyVB-Comm0	

uninstall:
	sudo rm -f /dev/virtualbot
	sudo modprobe -r virtualbot
#	sudo rm -f /lib/modules/`uname -r`/extra/virtualbot.ko
	sudo depmod

tests:
	sudo ./tests/tests_virtualbot_driver.py

test01:
	python3 ./javython.py send $(VIRTUALBOT_DEVICE) fffe0bgetPercepts

test02:
	python3 ./javython.py request $(VIRTUALBOT_DEVICE) fffe0bgetPercepts
# Javino header: fffeXY , where XY is the message length in hex
# TODO: read tem que retornar ...
# fffe49status(stopped);obstLeft(60);obstFront(49);obstRight(28);lightSensor(yes)

