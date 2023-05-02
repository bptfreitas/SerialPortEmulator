#!/usr/bin/python3
import serial

MSG = "Teste"

comm = serial.Serial( "/dev/ttyVB0", 9600, timeout = 0.2 )

comm.open
comm.isOpen

comm.write( bytes( MSG + "\n", "utf-8") ) 

comm.close