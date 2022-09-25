
KDIR ?= /lib/modules/`uname -r`/build

.PHONY: all clean setup-environment

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