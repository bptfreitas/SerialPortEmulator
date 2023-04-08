import sys
import serial
OP=sys.argv[1]
PORT=sys.argv[2]
MSG=sys.argv[3]
try:
	if(OP=='command' or OP=='send'):
		comm = serial.Serial(PORT, 9600, timeout=.1)
		comm.open
		comm.isOpen
		comm.write(bytes(MSG, 'utf-8'))
		comm.close
	if(OP=='request'):
		comm = serial.Serial(PORT, 9600, timeout=3)
		comm.open
		comm.isOpen
		comm.write(bytes(MSG, 'utf-8'))
		print (comm.readline().decode())
		comm.close
	if(OP=='listen'):
		comm = serial.Serial(PORT, 9600)
		comm.open
		comm.isOpen
		print (comm.readline().decode())
		comm.close
except Exception as inst:
	print ("Error on conect "+PORT)
	print(inst)
	sys.exit(1)
