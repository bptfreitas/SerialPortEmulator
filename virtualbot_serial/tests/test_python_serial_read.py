#!/usr/bin/python3
import serial

comm = serial.Serial( "/dev/ttyVBComm0", 9600 , timeout = 5 )

comm.open
comm.isOpen
print (comm.readline().decode())
comm.close