#!/usr/bin/python3

import sys
import serial

try:
	comm = serial.Serial("/dev/ttyVB0", 9600)
	
	# comm.open()
	
	# comm.close()
	
	sys.exit(0)
except Exception as inst:
	print(inst)
	sys.exit(-1)
