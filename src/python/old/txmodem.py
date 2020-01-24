#!/usr/bin/env python
#
# A Python class implementing the XMODEM and XMODEM-CRC send protocol.
#
# (C) 2012 Armin Tamzarian
# This software is distributed under a free software license, see LICENSE

import getopt
import math
import os
import re
import sys

from serial import *
from serial.tools import list_ports

class ExceptionTXMODEM(Exception):
    """ Base exception class for the TXMODEM class. """
    pass

class ConfigurationException(ExceptionTXMODEM):
    """ TXMODEM configuration exception. """
    pass

class CommunicationException(ExceptionTXMODEM):
    """ Unrecoverable TXMODEM communication exception. """
    pass

class TimeoutException(ExceptionTXMODEM):
    """ Possibly recoverable TXMODEM timeout exception. """
    pass

class UnexpectedSignalException(ExceptionTXMODEM):
    """ Unexpected communication response exception. """
    _signal = None;
    
    def __init__(self, message, signal):
        Exception.__init__(self, message)
        self._signal = signal
    
    def get_signal(self):
        return self._signal

class TXMODEM:
    """
    A Python class implementing the XMODEM and XMODEM-CRC send protocol built on top of `pySerial <http://pyserial.sourceforge.net/>`_.
    
    .. note:: TXMODEM objects should not utilize the :py:meth:`__init__` constructor and should instead be created via the :py:meth:`from_configuration` and :py:meth:`from_serial` methods.
    """
    
    # XMODEM standard defined parameters
    _SIGNAL_SOH   = chr(1)
    _SIGNAL_EOT   = chr(4)
    _SIGNAL_ACK   = chr(6)
    _SIGNAL_NAK   = chr(21)
    _SIGNAL_CAN   = chr(24)
    _SIGNAL_CRC16 = chr(67)
    
    _BLOCK_SIZE   = 128
    _RETRY_COUNT  = 10
    
    _PADDING_BYTE = chr(26)
    
    # default port configurations
    _configuration = {
        "port"     : None,
        "baudrate" : 115200,
        "bytesize" : EIGHTBITS,
        "parity"   : PARITY_NONE,
        "stopbits" : STOPBITS_ONE,
        "timeout"  : 10
    }

    # the pySerial port object
    _port = None
    
    # checksum calculation function
    _checksum = None

    # hooks for event callbacks
    EVENT_INITIALIZATION = 0
    """
    Event to be fired on initialization.
    
    ``function()``
    """
    EVENT_BLOCK_SENT     = 1
    """
    Event to be fired on a block sent.
    
    ``function(block_index, number_of_blocks)``
    """
    EVENT_TERMIATION     = 2
    """
    Event to be fired on termination.
    
    ``function()``
    """
    
    _event_callbacks = {
        EVENT_INITIALIZATION : [],
        EVENT_BLOCK_SENT     : [],
        EVENT_TERMIATION     : [],
    }
        
    @classmethod
    def from_configuration(cls, **configuration):
        """
        Class level static method for constructing the TXMODEM object from a set of configuration parameters.
        
        :param configuration: Configuration dictionary for the serial port as defined in the `pySerial API <http://pyserial.sourceforge.net/pyserial_api.html>`_.
        
        .. note:: If using this construction method the internal port object will be created and closed for each call to :py:meth:`send`.
        """
        cls_obj = cls()
        for k, v in configuration.items():
            cls_obj._configuration[k] = v
        return cls_obj

    @classmethod
    def from_serial(cls, serial):
        """
        Class level static method for constructing the TXMODEM object from a pySerial Serial object.
        
        :param port: A preconfigured pySerial Serial object.
        
        .. note:: If using this construction method the internal port object will not be reopened and shut down for each call to :py:meth:`send`.
        """
        cls_obj = cls()
        cls._port = serial
        return cls_obj
                            
    def add_callback(self, event_type, callback):
        """
        Add a callback for the specified event.
        
        :param event_type: Type of the event which should be one of the types: :py:const:`EVENT_INITIALIZATION`, :py:const:`EVENT_BLOCK_SENT`, or :py:const:`EVENT_TERMIATION`
        """
        self._event_callbacks[event_type].append(callback)
        
    def send(self, filename):
        """
        Execute the transmission of the file.
        
        :param filename: Filename of the file to transfer.
        
        :raises ConfigurationException: Will be raised in the event of an invalid file or port configuration parameter.
        :raises CommunicationException: Will be raised in the event of an unrecoverable serial communication error.
        """
        
        # Ensure proper preflight configuration
        if filename is None:
            raise ConfigurationException("No filename specified.")
        
        create_port = True
        if self._port != None:
            create_port = False 
        elif self._configuration is None or self._configuration["port"] is None:
            raise ConfigurationException("No serial port device specified.")
        
        # Open access to the input file
        input_file = None
        try:
            input_file = open(filename, "rb") 
        except IOError:
            raise ConfigurationException("Unable to access input filename '%s'." % (filename))
        
        # Open access to the serial device if necessary
        if create_port is True:
            try:
                self._port = Serial(**self._configuration)
            except ValueError:
                raise ConfigurationException("Invalid value for configuration parameters.")
            except SerialException:
                raise ConfigurationException("Unable to open serial device '%s' with specified parameters." % (configuration["port"]))

        try:            
            self._port.flush()
            self._execute_communication(self._initiate_transmission, "Unable to receive initial NAK.")            

            number_of_blocks = int(math.ceil(os.path.getsize(filename) / self._BLOCK_SIZE))
            for i in range(1, number_of_blocks + 1):
                block = input_file.read(self._BLOCK_SIZE)
                if block:
                    if len(block) < self._BLOCK_SIZE:
                        block += self._PADDING_BYTE * (self._BLOCK_SIZE - len(block))
                    self._execute_communication(self._transmit_block, "Maximum number of transmission retries exceeded.", **{"block_index": i, "block": block})            
                    self._trigger_callbacks(self.EVENT_BLOCK_SENT, **{"block_index" : i, "number_of_blocks" : number_of_blocks})
                    
            self._execute_communication(self._terminate_transmission, "Maximum number of termination retries exceeded.")
        except IOError:
            raise CommunicationException("Unexpected IO error.")
        finally:
            # Always remember to clean up after yourself
            input_file.close()
            
            if create_port and self._port is not None and self._port.isOpen():
                self._port.close()
                self._port = None
                
    def _trigger_callbacks(self, event_type, **args):
        """
        Trigger all callbacks for the given event type.
        
        :param event_type: Type of the event which should be one of the types: :py:const:`EVENT_INITIALIZATION`, :py:const:`EVENT_BLOCK_SENT`, or :py:const:`EVENT_TERMIATION`
        :param args" Arguments to pass to the callback function 
        """
        for event in self._event_callbacks[event_type]:
            event(**args)
                         
    def _set_crc_8(self, buffer):
        """
        Sets the _checksum calculation to _crc_8 for compatibility with _wait_for_signal.
        
        :param buffer: Exists for compatibility. Ignored.
        """
        self._checksum = self._crc_8
    
    def _set_crc_16(self, buffer):
        """
        Sets the _checksum calculation to _crc_16 for compatibility with _wait_for_signal.
        
        :param buffer: Exists for compatibility. Ignored.
        """
        self._checksum = self._crc_16
        
    def _crc_8(self, block):
        """
        Calculates the 8-bit CRC checksum as defined by the original XMODEM specification.
        
        :param block: The block for which the checksum will be calculated.
        """
        return chr(sum(map(ord, block)) & 0xFF)
    
    def _crc_16(self, block):
        """
        Calculates the 8-bit CRC checksum as defined by the original XMODEM-CRC specification.
        
        :param block: The block for which the checksum will be calculated.
        """
        crc = 0
        for b in block:
            crc = crc ^ (b << 8)
            for i in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc = crc << 1
        return "%c%c" % ((crc >> 8) & 0xFF, crc & 0xFF)
             
    def _wait_for_signal(self, signals):
        """
        Waits for a signal to be received and executes a specified callback function.
        
        :param signals: A dictionary with expected signals for keys and callback functions for values of the signature *function(buffer)*. 
        """
        buffer = (self._port.read()).decode("utf-8") 
        #print(buffer)
        while self._port.inWaiting():
            buffer += (self._port.read()).decode("utf-8") 
        self._port.flush()
		
        #print(signals.keys())
        
        if len(buffer) == 0:
            raise TimeoutException("Communication timeout expired.")
        elif buffer[0] in signals.keys():
            if signals[buffer[0]] is not None:
                signals[buffer[0]](buffer)
        else:
            raise UnexpectedSignalException("Unexpected communication signal received.", buffer)

    def _execute_communication(self, communication_function, failure_message, **args):
        """
        Abstract function to execute communication methods within the XMODEM error correction framework.
        
        :param communication_function: Communication function to execute within the error correction framework.
        :param failure_message: Failure message to raise along with the exception if applicable.
        :param args: Arguments to pass to the supplied communication_function.
        """
        for retry in range(self._RETRY_COUNT):
            try:
                communication_function(**args)
                return
            except UnexpectedSignalException as ex:
                if self._SIGNAL_CAN in ex.get_signal():
                    raise CommunicationException("CAN signal received. Transmission forcefully terminated by receiver.")
            except (TimeoutException, SerialException):
                pass
            
        raise CommunicationException(failure_message)
            
    def _initiate_transmission(self):
        """
        Initiates the transmission while automatically selecting between XMODEM and XMODEM-CRC modes.
        """
        try:
            self._wait_for_signal({
                 self._SIGNAL_NAK : self._set_crc_8,
                 self._SIGNAL_CRC16 : self._set_crc_16
            })
            self._trigger_callbacks(self.EVENT_INITIALIZATION)
        except UnexpectedSignalException as ex:
            raise CommunicationException("Unknown initiation signal received.")    
    
    def _transmit_block(self, block_index, block):
        """
        Transmits the specified block of data.
        
        :param block_index: Index of the block to be transmitted.
        :param block: Buffered block of data to be transmitted.
        """
        try:
            self._port.write(("%c%c%c%s%s" % (self._SIGNAL_SOH, chr(block_index & 0xFF), chr(~block_index & 0xFF), block, self._checksum(block))).encode())
            self._wait_for_signal({self._SIGNAL_ACK: None})
            return
        except UnexpectedSignalException as ex:
            if self._SIGNAL_CAN in ex.get_signal():
                raise CommunicationException("CAN signal received. Transmission forcefully terminated by receiver.")
        except (TimeoutException, SerialException):
            pass
            
    def _terminate_transmission(self):
        """
        Terminates the XMODEM transmission.
        """
        self._port.write((self._SIGNAL_EOT).encode())
        self._wait_for_signal({self._SIGNAL_ACK: None})
        self._port.flush()
        
        self._trigger_callbacks(self.EVENT_TERMIATION)
        
class Main:
    """
    A Python Main-style class for command line execution of the TXMODEM functionality.
    
    :Examples:
    - ``python txmodem.py --file [filename] --port [port]``
    - ``python -m txmodem.txmodem --file [filename] --port [port]``
    """

    _EXIT_OK = 0
    _EXIT_ERROR = 1
    
    # default port configurations
    _configuration = {
        "port" : None,
        "baudrate" : 115200,
        "bytesize" : EIGHTBITS,
        "parity" : PARITY_NONE,
        "stopbits" : STOPBITS_ONE,
        "timeout" : 10
    }
    
    _tx_filename = None
        
    def __init__(self):
        """
        Main class constructor.
        """
        sys.exit(self._run())
        
    def _list_ports(self):
        """
        Lists the accessible serial devices and their associated system names.
        """
        print("Currently available ports:")
        print("--------------------------")
        for port in list_ports.comports():
            print("  %s" % (port[0]))    
    
    def _usage(self):
        """
        Prints the usage information for command line execution.
        """
        print('''\
    XMODEM transfer utility
    
    Usage: python txmodem.py [OPTION]...''')
    
    def _help(self): 
        """
        Prints the usage and help information for command line execution.
        """
        self._usage()
        
        print('''
    Startup:
      -?, --help    print this help
      -l, --list    list the available serial port devices
    
    Configuration:
      -p, --port    specify the serial port device to use
      -b, --baud    specify the baud rate for the serial port device
      -t, --timeout specify the communication timeout in s

    Transfer:
      -f, --file    specify the file that will be transfered
     ''')
    
    def _callback_initialized(self):
        print("Transfer initialization complete.")
        
    def _callback_block_sent(self, block_index, number_of_blocks):
        print("Sent block [ %d / %d ]" % (block_index, number_of_blocks))
        
    def _callback_terminated(self):
        print("Transfer successfully terminated.")
                        
    def _run(self):
        """
        Main function to initiate execution of the class.
        """
        # scan arguments for options    
        try:
            opts, args = getopt.getopt(sys.argv[1:], "?lp:b:t:f:", ["help", "list", "port=", "baud=", "timeout=", "file="])
			
        except getopt.GetoptError as err:
            print(str(err))
            return self._EXIT_ERROR
        
        # initial argument scan for execution terminators
        for o, a in opts:
            if o in ("-?", "--help"):
                self._help()
                return self._EXIT_OK
            elif o in ("-l", "--list"):
                self._list_ports()
                return self._EXIT_OK
            
        # argument scan to extract configuration items
        for o, a in opts:
            if o in ("-p", "--port"):
                self._configuration["port"] = a;
            elif o in ("-b", "--baud"):
                try:
                    self._configuration["baudrate"] = int(a)
                except ValueError:
                    print("[ERROR] Invalid baud rate '%s' specified." % (a))
                    return self._EXIT_ERROR 
            elif o in ("-t", "--timeout"):
                try:
                    self._configuration["timeout"] = int(a)
                except ValueError:
                    print("[ERROR] Invalid timeout '%s' specified." % (a))
                    return self._EXIT_ERROR 
            elif o in ("-f", "--file"):
                self._tx_filename = a
                
        try:
            tx_object = TXMODEM.from_configuration(**self._configuration)
            
            tx_object.add_callback(TXMODEM.EVENT_INITIALIZATION, self._callback_initialized)
            tx_object.add_callback(TXMODEM.EVENT_BLOCK_SENT, self._callback_block_sent)
            tx_object.add_callback(TXMODEM.EVENT_TERMIATION, self._callback_terminated)
            
            tx_object.send(self._tx_filename)
        except(ConfigurationException, CommunicationException) as ex:
            print("[ERROR] %s" %(ex))        
        except(KeyboardInterrupt, SystemExit):
            print("[INFO] Exit command detected.")
        
        return self._EXIT_OK

if __name__ == "__main__":
    Main()