#!/usr/bin/python3

import sys
import serial
import re

# This class is to read the VirtualBot device header and set the test parameters accordienly

class ReadVirtualBotParameters:
    
    def __init__(self):

        self.__fileName = '../include/virtualbot.h'

        with open(self.__fileName, 'r') as f:
            contents = f.read();




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
