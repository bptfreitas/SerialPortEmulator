#!/usr/bin/python3
import serial

comm = serial.Serial( "/dev/ttyVBComm0", 9600 , timeout = None )

comm.open
comm.isOpen
print (comm.readline().decode())
comm.close