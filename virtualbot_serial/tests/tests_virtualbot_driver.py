#!/usr/bin/python3

import sys
import serial
import re
import subprocess

# This class is to read the VirtualBot device header and set the test parameters accordienly

class VirtualBotParameters:
    
    def __init__(self):

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

        sys.stdout.write( "VirtualBot compile-time parameters: \n" )
        for directive in self.__parameters.keys(): 
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

import unittest

class TestSerialObject(unittest.TestCase):


    def setUp(self):

        # Clearing the kernel log for the tests
        subprocess.run([ "sudo" , "dmesg" , "-C" ])

        self.__VBParams = VirtualBotParameters()

        

	# Check is you can instantiate a Serial object with the VirtualBot driver
    def test_01_VirtualBotSerialObjectInstantiated(self):
        self.assertIsInstance( serial.Serial("/dev/ttyVB0", 9600) , serial.Serial )

    def test_02_VBCommSerialObjectInstantiated(self):        

        self.assertIsInstance( serial.Serial("/dev/ttyVB-Comm0", 9600) , serial.Serial )

    def test_03_ReadLine_VB_to_VBComm(self):
        comm1 = serial.Serial("/dev/ttyVB0", 9600, timeout=3)
        comm2 = serial.Serial("/dev/ttyVB-Comm0", 9600, timeout=3)

        comm1.write( bytes("XYZ", 'utf-8') )

        print ( comm2.readline().decode() )




    #def test_03_VirtualBotWrite(self):

        



    #def test_isupper(self):
    #    self.assertTrue('FOO'.isupper())
    #    self.assertFalse('Foo'.isupper())

"""     def test_split(self):
        s = 'hello world'
        self.assertEqual(s.split(), ['hello', 'world'])
        # check that s.split fails when the separator is not a string
        with self.assertRaises(TypeError):
            s.split(2)
 """

if __name__ == '__main__':
    unittest.main()
