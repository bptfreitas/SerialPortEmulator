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

                # print( contents )

                try:                    
                    index = contents.index("#define")
                except ValueError:
                    sys.stderr.write( "Not a #define\n")
                    continue
                
                try:
                    constant = contents[ index + 1 ].strip()                    
                except Exception as err:
                    sys.stderr.write("Invalid constant: " + str(err) + '\n')

                try:
                    value = int( contents[ index + 2 ].strip() )
                    self.__parameters[ constant  ] = value
                    continue
                except Exception as err:
                    sys.stderr.write("Not an integer: " + str(err) + '\n')
                
                try:
                    value = str( ' '.join( [ x.strip() for x in contents[ index + 2 : ] ] ) )
                    self.__parameters[ constant  ] = value
                    continue        
                except Exception as err:
                    sys.stderr.write("Not a string: " + str(err) + '\n')                

                # now for the comm part
                    
        sys.stdout.write( "Serial Emulator compile-time parameters: \n" )

        for directive in self.__parameters.keys(): 

            sys.stdout.write( directive + "\t" + str( self.__parameters[directive] ) + "\n" )

        sys.stdout.write( "\n" )

    def __getitem__(self, index):
        return self.__parameters[index]



def read_serial_port( read_var , serial_object ):

    read_var[ 'value' ] = serial_object.readline().decode()

class TestSerialObject(unittest.TestCase):
       
    @classmethod
    def setUpClass(self):

        print("setUpClass")

        # reading compile-time parameters
        self.__VBParams = VirtualBotParameters(  )

        self.__EmulatedPort = "/dev/ttyEmulatedPort"

        self.__Exogenous = "/dev/ttyExogenous"
        

    def setUp(self):

        # Clearing the kernel log for the tests
        subprocess.run( [ "sudo" , "dmesg" , "-C" ] )

    def tearDown(self):

        filename = "exec{0}.log".format( 0 )

        with open(filename, "w") as output:

            ret = subprocess.run([ "sudo" , "dmesg" , "-T" ], 
                capture_output=True )

            output.write( ret.stdout.decode("utf-8") )
        

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

        data_to_test = ["XYZ\n" , "ABC\n"]

        for data in data_to_test:

            read_thread = threading.Thread(target=read_serial_port, \
                args=( data_in, comm2 ))

            read_thread.start()

            time.sleep(2)

            comm1.write( bytes( data , 'utf-8') )

            read_thread.join()

            self.assertEqual( data_in[ 'value' ]  , data )

        comm1.close()
        comm2.close()


    def test_05_Exogenous_Write_on_EmulatedPort(self):

        comm1 = serial.Serial( str( self.__Exogenous + "0" ) , 
            9600, 
            timeout = None )

        comm2 = serial.Serial( str( self.__EmulatedPort + "0" ), 
            9600, 
            timeout = 3 )            

        data_in = {}

        data_to_test = ["XYZ\n" , "ABC\n"]

        for data in data_to_test:

            read_thread = threading.Thread(target=read_serial_port, \
                args=( data_in, comm2 ))

            read_thread.start()

            time.sleep(2)

            comm1.write( bytes( data , 'utf-8') )

            read_thread.join()

            self.assertEqual( data_in[ 'value' ]  , data )

        comm1.close()
        comm2.close()

    @unittest.expectedFailure
    def test_06_EmulatedPort_ErrorWhenWritingWithExogenousClosed(self):

        comm1 = serial.Serial( str( self.__EmulatedPort + "0" ), 
            9600, 
            timeout = 3 )

        comm1.write( bytes("XYZ\n", 'utf-8') )

    @unittest.expectedFailure
    def test_07_Exogenous_ErrorWhenWritingWithEmulatedPortClosed(self):

        comm1 = serial.Serial( str( self.__Exogenous + "0" ), 
            9600, 
            timeout = 3 )

        comm1.write( bytes("XYZ\n", 'utf-8') )
            
if __name__ == '__main__':
    unittest.main()
