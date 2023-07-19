# SerialPortEmulator

This is a serial port emulator. This is done by creating pairs of ports on /dev where writing data on one port can be read on the pair and vice-versa.


## Installation

1) Install the dependencies. On your Debian-like Linux machine, run:

```
sudo apt install python3-serial linux-headers-`uname -r` gcc binutils make
```

2) Inside the driver folder, run:

```
make clean all
sudo make modules_install
sudo make install
```

3) Set read and write permissions on the pairs of devices to be used

Example: for the EmulatedPort 0 device:

```
sudo chmod a+rw /dev/ttyEmulatedPort0
sudo chmod a+rw /dev/ttyExogenous0
```

## Uninstallation

Inside the 'driver' folder, run: 

```
sudo make uninstall
```

## Use 

After the installation, by default, it will be instatiated on /dev many pairs of devices:

- /dev/ttyEmulatedPort0 <---> /dev/ttyExogenous0
- /dev/ttyEmulatedPort1 <---> /dev/ttyExogenous1
...

And so on. Writing on one device will make its content to be read on the other pair, and vice-versa

You MUST at least execute a read operation on the Exogenous port to make the OS create the necessary structures

Example:
1) Open a terminal window and run:
```
cat /dev/ttyExogenous0
```
The device will be put on waiting for incoming data

2) Open another terminal window and run:
```
echo "XYZ" > /dev/ttyEmulatedPort0
```
Return to the first terminal Window. It should appear the 'XYZ' on it.
