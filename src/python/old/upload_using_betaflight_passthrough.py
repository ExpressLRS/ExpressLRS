import sys
import glob
import serial
import time
from xmodem import XMODEM

result = []

BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


def getc(size, timeout=8):
    gbytes = ser.read(size)
    print(f'Read Byte: {gbytes}')
    return gbytes or None


def putc(data, timeout=8):
    pbytes = ser.write(data)
    print(f'Put Byte: {pbytes}')
    return pbytes or None


if __name__ == '__main__':
    print(serial_ports())
    modem = XMODEM(getc, putc)

print("Going to try using "+ result[0])

ser = serial.Serial()
ser.baudrate = 420000
ser.port = result[0]
ser
ser.open()

ser.write(BootloaderInitSeq1)
time.sleep(0.5)
ser.write(BootloaderInitSeq2)
time.sleep(1)
ser.flush()

stream = open('firmware.bin', 'rb')
modem.send(stream)

ser.close()             # close port
