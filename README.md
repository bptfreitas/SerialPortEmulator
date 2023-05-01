# VirtualBot
This is a simulator device for MAS systems communicating via Javino.

# Installation

1) Install the dependencies. On your Debian-like Linux machine, run:

```
sudo apt install python3-serial linux-headers-`uname -r` gcc binutils make
```

2) Inside the virtualbot_serial folder, run:

```
make clean all
sudo make modules_install
sudo make install
```

# Uninstallation

Inside the 'virtualbot_serial' folder, run: 

```
sudo make uninstall
```

# Use 

After the installation, by default, it will be instatiated on /dev many pairs of devices:

/dev/ttyVB0 <---> /dev/ttyVBComm0
/dev/ttyVB1 <---> /dev/ttyVBComm1
...

And so on. Writing on one device will make its content to be read on the other pair, and vice-versa

Example:
1) Open a terminal Window and run:
```
cat /dev/ttyVB0
```
The device will be put on waiting for incoming data

2) Open another terminal Windows and run:
```
echo "XYZ" > /dev/ttyVBComm0
```
Return to the other terminal Window. It should appear the 'XYZ' on it.














