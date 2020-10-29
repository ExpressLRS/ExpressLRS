Import("env")
import serial
from xmodem import XMODEM
import time
import sys, glob
import serials_find
import subprocess

requestedBaudrate = 57600
requestedParity = "8E1"

port = serials_find.get_serial_port()
print("Going to use %s\n" % port)

s = serial.Serial(port=port, baudrate=115200, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

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
    try:
        inChars += s.read().decode('utf-8')
    except UnicodeDecodeError:
        # just discard possible RC / TLM data
        pass


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
s.write(("serialpassthrough "+str(SerialInfoSplit[SerialRXindex][1])+" "+str(requestedBaudrate)+" 0 "+str(requestedParity)+'\n').encode())
sys.stdout.write(("serialpassthrough "+str(SerialInfoSplit[SerialRXindex][1])+" "+str(requestedBaudrate)+" 0 "+str(requestedParity)+'\n'))
time.sleep(1)


sys.stdout.flush()
s.flush()

s.close() # Must be closed for stm32flash to open

print()
print()

subprocess.call([env['PROJECT_PACKAGES_DIR']+'/tool-stm32duino/stm32flash/stm32flash', port, '-k'])
subprocess.call([env['PROJECT_PACKAGES_DIR']+'/tool-stm32duino/stm32flash/stm32flash', port, '-u'])
subprocess.call([env['PROJECT_PACKAGES_DIR']+'/tool-stm32duino/stm32flash/stm32flash', port, '-w', 'bootloader\Bootloader.hex'])

sys.exit()
