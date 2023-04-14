#!/usr/bin/python3

import sys
import serial
import re

# This class is to read the VirtualBot device header and set the test parameters accordienly

class VirtualBotParameters:
    
    def __init__(self):

        self.__parameters = {}

        self.__fileName = '../include/virtualbot.h'

        with open(self.__fileName, 'r') as f:
            for line in f.read():

                contents = line.split(" ").strip()

                try:
                    index = contents.index("#define")
                except ValueError:
                    continue

                if contents[ index + 1 ].strip() == "VIRTUALBOT_MAX_SIGNAL_LEN":
                    self.__parameters["VIRTUALBOT_MAX_SIGNAL_LEN"] = int( contents[ index + 1 ].strip() )

                elif contents[ index + 1 ].strip() == "VIRTUALBOT_TOTAL_SIGNALS":
                    self.__parameters["VIRTUALBOT_TOTAL_SIGNALS"] = int( contents[ index + 1 ].strip() )

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

	# Check is you can instantiate a Serial object with the VirtualBot driver
    def test_01_SerialObjectInstantiated(self):
        self.assertIsInstance( serial.Serial("/dev/ttyVB0", 9600) , serial.Serial )

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
