import serial
from xmodem import XMODEM
import time
import sys

import serials_find

def dbg_print(line=''):
    sys.stdout.write(line + '\n')
    sys.stdout.flush()


def bf_passthrough_init(port, requestedBaudrate):
    sys.stdout.flush()
    dbg_print("======== PASSTHROUGH INIT ========")
    dbg_print("  Going to use %s @ %s" % (port, requestedBaudrate))

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

    dbg_print("\nAttempting to detect Betaflight UART configuration...")

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

    dbg_print()

    if uartNumber != -1:
        dbg_print("Detected Betaflight Serial RX config: " +str(serialInfo[SerialRXindex]))
    else:
        dbg_print("Failed to make contact with Betaflight, possibly already in passthrough mode?")
        dbg_print("If the next step fails please reboot FC")
        dbg_print()
        raise Exception("BF connection failed");

    cmd = "serialpassthrough "+str(SerialInfoSplit[SerialRXindex][1])+" "+str(requestedBaudrate)

    dbg_print("Setting serial passthrough...")
    dbg_print("  CMD: '%s'" % cmd)
    s.write((cmd + '\n').encode())
    time.sleep(1)

    s.flush()
    s.close()

    dbg_print("========================================================\n")


if __name__ == '__main__':
    try:
        requestedBaudrate = int(sys.argv[1])
    except:
        requestedBaudrate = 420000
    port = serials_find.get_serial_port()
    bf_passthrough_init(port, requestedBaudrate)
