# CarrinhoSimulator
This is a simulator device for MAS systems communicating via Javino.

# INSTALLATION

1) On your Debian-like Linux machine, run:

`[sudo] apt install python3-serial linux-headers-\`uname -r\` gcc binutils make ` 

2) Inside the virtualbot_serial folder:

make clean all
make modules_install
make install

It will then be instantiated /dev/ttyVM* devices

# UNINSTALATION

1) run 'make uninstall' inside the folder virtualbot_serial
