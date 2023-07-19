#!/usr/bin/python3

import sys
import serial
import re
import subprocess
import time

import threading

import unittest

from collections import UserDict


# This class is to read the VirtualBot device header and set the test parameters accordienly

class VirtualBotParameters(dict):
    
    def __init__(self,*arg,**kw):
        super(VirtualBotParameters, self).__init__(*arg, **kw)

        self.__parameters = {}

        self.__fileName = 'include/virtualbot.h'

        with open(self.__fileName, 'r') as f:

            for line in f.readlines():

                contents = [ directive.strip() for directive in line.split(" ") ]

                try:                    
                    index = contents.index("#define")
                except ValueError:
                    continue

                if contents[ index + 1 ].strip() == "VIRTUALBOT_MAX_SIGNAL_LEN":
                    self.__parameters["VIRTUALBOT_MAX_SIGNAL_LEN"] = int( contents[ index + 2 ].strip() )

                elif contents[ index + 1 ].strip() == "VIRTUALBOT_TOTAL_SIGNALS":
                    self.__parameters["VIRTUALBOT_TOTAL_SIGNALS"] = int( contents[ index + 2 ].strip() )

                elif contents[ index + 1 ].strip() == "VIRTUALBOT_MAX_TTY_MINORS":
                    self.__parameters["VIRTUALBOT_MAX_TTY_MINORS"] = int( contents[ index + 2 ].strip() )

                elif contents[ index + 1 ].strip() == "VIRTUALBOT_TTY_MAJOR":
                    self.__parameters["VIRTUALBOT_TTY_MAJOR"] = int( contents[ index + 2 ].strip() )

                elif contents[ index + 1 ].strip() == "VIRTUALBOT_TTY_NAME":
                    self.__parameters["VIRTUALBOT_TTY_NAME"] = contents[ index + 2 ].strip()

                elif contents[ index + 1 ].strip() == "VB_COMM_TTY_NAME":
                    self.__parameters["VB_COMM_TTY_NAME"] = contents[ index + 2 ].strip()                    

                # now for the comm part

        self = self.__parameters                
        
        sys.stdout.write( "Serial Emulator compile-time parameters: \n" )

        for directive in self.keys(): 
            sys.stdout.write( directive + "\t" + str(self[directive])  + "\n" )

        sys.stdout.write( "\n" )

    def __getitem__(self, index):
        return self.__parameters[index]


""" try:
	comm = serial.Serial("/dev/ttyVB0", 9600)
	
	# comm.open()
	
	# comm.close()
	
	sys.exit(0)

except Exception as inst:
	print(inst)
	sys.exit(-1)
 """

def read_serial_port( read_var , serial_object ):

    read_var[ 'value' ] = serial_object.readline().decode()


class TestSerialObject(unittest.TestCase):
       
    @classmethod
    def setUpClass(self):

        print("setUpClass")

        # reading compile-time parameters
        self.__VBParams = VirtualBotParameters( debug = False )

        self.__EmulatedPort = "/dev/ttyEmulatedPort"

        self.__Exogenous = "/dev/ttyExogenous"
        

    def setUp(self):

        # Clearing the kernel log for the tests
        subprocess.run( [ "sudo" , "dmesg" , "-C" ] )        
        

	# Check is you can instantiate a Serial object with the VirtualBot driver
    def test_01_VirtualBotSerialObjectInstantiated(self):

        self.assertIsInstance( 
            serial.Serial( str( self.__EmulatedPort + "0" ) ,
                9600) , 
                serial.Serial 
            )

    def test_02_VBCommSerialObjectInstantiated(self):        

        self.assertIsInstance( 
            serial.Serial( str( self.__Exogenous + "0" ), 
                9600) , 
                serial.Serial 
            )

    def test_03_checkOpenStatuses(self):

        self.skipTest("Not implemented")

        comm1 = serial.Serial( str( self.__EmulatedPort + "0" ), 
            9600)

        comm2 = serial.Serial( str( self.__Exogenous + "0" ) , 9600)


    def test_04_EmulatedPort_Write_on_Exogenous(self):

        comm1 = serial.Serial( str( self.__EmulatedPort + "0" ), 
            9600, 
            timeout = 3 )

        comm2 = serial.Serial( str( self.__Exogenous + "0" ) , 
            9600, 
            timeout = None )

        data_in = {} 

        read_thread = threading.Thread(target=read_serial_port, args=( data_in, comm2 ))

        read_thread.start()

        # recv = comm2.readline().decode()

        time.sleep(2)


        comm1.write( bytes("XYZ\n", 'utf-8') )

        # print ( comm2.readline().decode() )

        read_thread.join()

        self.assertEqual( data_in[ 'value' ]  , "XYZ\n" )


    def test_05_Exogenous_Write_on_EmulatedPort(self):

        comm1 = serial.Serial( str( self.__Exogenous + "0" ), 
            9600, 
            timeout=3)

        comm2 = serial.Serial( str( self.__EmulatedPort + "0" ) , 
            9600, 
            timeout = None )

        data_in = {} 

        read_thread = threading.Thread( target=read_serial_port, 
            args=( data_in, comm2 )
            )

        read_thread.start()

        # recv = comm2.readline().decode()

        time.sleep(2)

        comm1.write( bytes("XYZ\n", 'utf-8') )

        # print ( comm2.readline().decode() )

        read_thread.join()

        self.assertEqual( data_in[ 'value' ]  , "XYZ\n" )

if __name__ == '__main__':
    unittest.main()
