import serial
from xmodem import XMODEM
import time
import sys, glob

result = []


try:
    requestedBaudrate = int(sys.argv[1])
except:
    requestedBaudrate = 420000

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

print(serial_ports())
sys.stdout.write("Dected the following serial ports on this system: \n")
sys.stdout.write(str(serial_ports())[1:-1])
sys.stdout.write("\n")

if(len(result) == 0):
    raise EnvironmentError('No valid serial port detected or port already open')

print("Going to try using "+ result[0])

s = serial.Serial(port=result[0], baudrate=115200, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

s.write(chr(0x23).encode())
time.sleep(1)
s.write(("serial\n").encode())
time.sleep(1)

lines = []
inChars = ""
serialInfo = []
SerialInfoSplit = []

SerialRXindex = -1
uartNumber = -1
print()
print()
print("Attempting to detect Betaflight UART configuration...")

while s.in_waiting:
    inChars += s.read().decode('utf-8')


for line in inChars.split('\n'):
    if "serial" in line:
        lines.append(line)
        sys.stdout.write(line[0:len(line)-1])
        sys.stdout.write("\n")

sys.stdout.flush()


for line in lines:
    if line[0:6] == "serial":
        serialInfo.append(line)

for line in serialInfo:
    data = line.split()
    SerialInfoSplit.append(data)

for i in range(0,len(serialInfo)):
    data = SerialInfoSplit[i][2]
    if(data == "64"):
        uartNumber = data
        SerialRXindex = i

sys.stdout.write('\n')

if uartNumber != -1:
    print("Detected Betaflight Serial RX config: " +str(serialInfo[SerialRXindex]))
else:
    sys.stdout.write("Failed to make contact with Betaflight, possibly already in passthrough mode?\n")
    sys.stdout.write("If the next step fails please reboot FC\n")
    sys.stdout.write('\n')
    sys.stdout.flush()
    sys.exit()

sys.stdout.write("Setting serial passthrough...\n")
s.write(("serialpassthrough "+str(SerialInfoSplit[SerialRXindex][1])+" "+str(requestedBaudrate)+'\n').encode())
sys.stdout.write(("serialpassthrough "+str(SerialInfoSplit[SerialRXindex][1])+" "+str(requestedBaudrate)+'\n'))
time.sleep(1)


sys.stdout.flush()
s.flush()

print()
print()
