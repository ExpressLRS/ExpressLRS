import serial
from xmodem import XMODEM
import time
import sys

result = []

filename = ''

BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

def StatusCallback(total_packets, success_count, error_count):
    if(total_packets % 10 == 0):
        print("Pkts: " + str(total_packets) + " err: " +str(error_count))

def getc(size, timeout=1):
    return s.read(size)

def putc(data, timeout=1):
    s.write(data)

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

if __name__ == '__main__':
    print(serial_ports())

print("Going to use "+ result[0])

s = serial.Serial(port=result[0], baudrate=420000, bytesize=8, parity='N', stopbits=1, timeout=None, xonxoff=0, rtscts=0)

s.write(BootloaderInitSeq1)
time.sleep(1)
s.write(BootloaderInitSeq2)

s.close()
time.sleep(1)
s.open()

try:
    filename = sys.argv[1]
except:
    filename = 'firmware.bin'
    
    
modem = XMODEM(getc, putc)
stream = open(filename, 'rb')
status = modem.send(stream, retry=8, callback=StatusCallback)

if(status):
    print("Success!!!!")
else:
    print("FAILED")

s.close()
stream.close()
