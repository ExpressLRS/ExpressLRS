import serial
from xmodem import XMODEM
import time
import sys
import logging
import os, glob

logging.basicConfig(level=logging.ERROR)

result = []

filename = ''
filesize = 0
filechunks = 0

BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

sys.stdout.flush()

try:
    filename = sys.argv[1]
except:
    dbg_print("Filename not provided, going to use default firmware.bin")
    filename = "firmware.bin"

if not os.path.exists(filename):
    msg = "[FAILED] bin file '%s' does not exist\n" % filename
    dbg_print(msg)
    raise EnvironmentError(msg)

def dbg_print(line):
    sys.stdout.write(line)
    sys.stdout.flush()

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

if not result:
    msg = "\n[FAILED] Cannot find suitable serial port\n\n"
    dbg_print(msg)
    raise EnvironmentError(msg)

dbg_print("Going to use "+ result[0]+"\n")

s = serial.Serial(port=result[0], baudrate=420000, bytesize=8, parity='N', stopbits=1, timeout=.5, xonxoff=0, rtscts=0)

s.timeout = 1.
try:
    already_in_bl = s.read(3).decode('utf-8')
except UnicodeDecodeError:
    already_in_bl = ""

if 'CC' not in already_in_bl:
    s.timeout = .5
    s.write_timeout = .5

    currAttempt = 0
    gotBootloader = False

    dbg_print("\nAttempting to reboot into bootloader...\n")

    while gotBootloader == False:

        currAttempt += 1
        dbg_print("[%2u] retry...\n" % currAttempt)
        time.sleep(.5)
        if 10 < currAttempt:
            msg = "Failed to get to BL in reasonable time\n"
            dbg_print(msg)
            raise EnvironmentError(msg)

        # request reboot
        s.write(BootloaderInitSeq1)
        s.flush()
        start = time.time()
        while ((time.time() - start) < 2):
            try:
                line = s.readline().decode('utf-8')
                if not line and s.in_waiting:
                    line = s.read(1000).decode('utf-8')
            except UnicodeDecodeError:
                continue
            #dbg_print("line : '%s'\n" % (line.strip(), ))
            if "'2bl', 'bbb'" in line:
                # notify bootloader to start uploading
                s.write(BootloaderInitSeq2)
                s.flush()
                dbg_print("Got into bootloader after: %u attempts\n" % (currAttempt))
                gotBootloader = True
                break

    # change timeout to 30sec
    s.timeout = 30.
    s.write_timeout = 5.

    # sanity check! Make sure the bootloader is started
    start = time.time()
    while True:
        char = s.read().decode('utf-8')
        if char == 'C':
            break
        if ((time.time() - start) > 10):
            msg = "[FAILED] Unable to communicate with bootloader...\n"
            dbg_print(msg)
            raise EnvironmentError(msg)
else:
    dbg_print("\nWe were already in bootloader\n")

# change timeout to 5sec
s.timeout = 5.
s.write_timeout = 5.

# open binary
stream = open(filename, 'rb')
filesize = os.stat(filename).st_size
filechunks = filesize/128

dbg_print("uploading %d bytes...\n" % (filesize,))

def StatusCallback(total_packets, success_count, error_count):
    sys.stdout.flush()
    if (total_packets % 10 == 0):
        if (error_count > 0):
            dbg_print(str(round((total_packets/filechunks)*100)) + "% err: " + str(error_count) + "\n")
        else:
            dbg_print(str(round((total_packets/filechunks)*100)) + "%\n")

def getc(size, timeout=3):
    return s.read(size) or None

def putc(data, timeout=3):
    return s.write(data)

modem = XMODEM(getc, putc, mode='xmodem')
#modem.log.setLevel(logging.DEBUG)
status = modem.send(stream, retry=10, callback=StatusCallback)

s.close()
stream.close()

if (status):
    dbg_print("Success!!!!\n\n")
else:
    dbg_print("[FAILED] Upload failed!\n\n")
    raise EnvironmentError('Failed to Upload')
