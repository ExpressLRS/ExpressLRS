import serial
from xmodem import XMODEM
import time
import sys
import logging
import os

logging.basicConfig(level=logging.ERROR)

result = []

filename = ''
filesize = 0
filechunks = 0

BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

sys.stdout.flush()

def StatusCallback(total_packets, success_count, error_count):
    #sys.stdout.write(".")
    sys.stdout.flush()
    
    if(total_packets % 10 == 0):
        
        if(error_count > 0):
            sys.stdout.write(str(round((total_packets/filechunks)*100)) + "% err: " + str(error_count) + "\n")
        else:
            sys.stdout.write(str(round((total_packets/filechunks)*100)) + "%\n")
            
        sys.stdout.flush()

def getc(size, timeout=3):
    return s.read(size)

def putc(data, timeout=3):
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

serial_ports()

#sys.stdout.write("Dected the following serial ports on this system: \n")
#sys.stdout.write(str(serial_ports())[1:-1])
#sys.stdout.write("\n")

sys.stdout.write("Going to use "+ result[0] + "\n")

s = serial.Serial(port=result[0], baudrate=420000, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

alreadyInBootloader = False
s.flush()
time.sleep(1)


reading = s.read(3).decode('utf-8')
	
if('C' in reading):
    alreadyInBootloader = True

MaxBootloaderAttempts = 20
currAttempt = 0
gotBootloader = False

if(alreadyInBootloader == False):
    
    sys.stdout.write("\nAttempting to reboot into bootloader...\n")
    
    while gotBootloader == False:
        
        s.flush()
        s.write(BootloaderInitSeq1)
        time.sleep(0.5)

        lines = []
        inChars = ""

        while s.in_waiting:
            inChars += s.read().decode('utf-8')

        for line in inChars.split('\n'):
            if "UART Bootloader for ExpressLRS" in line:
                sys.stdout.write("Got into bootloader after: ")
                sys.stdout.write(str(currAttempt+1))
                sys.stdout.write(" attempts\n")
                sys.stdout.flush()
                gotBootloader = True
                break
            
        if(currAttempt == 20):
            sys.stdout.write("Failed to get to BL in reasonable time\n")
            raise SystemExit
            break
        
        currAttempt = currAttempt + 1

    s.write(BootloaderInitSeq2)

else:
    sys.stdout.write("\nWe were already in bootloader\n")
    sys.stdout.flush()
    

time.sleep(0.2)
s.close()
s.open()


try:
    filename = sys.argv[1]
except:
    print("Filename not provided, going to use default firmware.bin")
    filename = "firmware.bin"

stream = open(filename, 'rb')
filesize = os.stat(filename).st_size
filechunks = filesize/128

sys.stdout.write("uploading ")
sys.stdout.write(str(filesize)+" bytes...\n")
sys.stdout.flush()
                
modem = XMODEM(getc, putc)
status = modem.send(stream, retry=10, callback=StatusCallback)

if(status):
    print("100%\nSuccess!!!!")
else:
    print("FAILED")
    raise EnvironmentError('Failed to Upload')

s.close()
stream.close()
