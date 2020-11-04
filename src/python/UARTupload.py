import serial
from xmodem import XMODEM
import time
import sys
import logging
import os
import serials_find

logging.basicConfig(level=logging.ERROR)

filename = ''
filesize = 0
filechunks = 0

BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
BootloaderInitSeq1_patch = bytes([0xEC,0x04,0x32,0x62,0x6c,0x41]) #needed to fix a bug, possibly remove in a few weeks once everyone has updated.
BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

sys.stdout.flush()

def dbg_print(line):
    sys.stdout.write(line)
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

port = serials_find.get_serial_port()
dbg_print("Going to use %s\n" % port)
s = serial.Serial(port=port, baudrate=420000, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

try:
    already_in_bl = s.read(2).decode('utf-8')
except UnicodeDecodeError:
    already_in_bl = ""
    
if 'CC' not in already_in_bl:
    currAttempt = 0
    gotBootloader = False

    dbg_print("\nAttempting to reboot into bootloader...\n")

    while gotBootloader == False:

        currAttempt += 1
        dbg_print("[%1u] retry...\n" % currAttempt)
        time.sleep(.5)
        if 10 < currAttempt:
            msg = "[FAILED] to get to BL in reasonable time\n"
            dbg_print(msg)
            raise EnvironmentError(msg)

        # request reboot
        line = ""
        if (currAttempt % 2) == 0:
            s.write(BootloaderInitSeq1)
        else:
            s.write(BootloaderInitSeq1_patch)
        time.sleep(0.5)

        start = time.time()
        while ((time.time() - start) < 2):
            try:
                if (s.in_waiting > 0):
                    line += s.readline().decode('utf-8')
            except UnicodeDecodeError:
                continue
            #dbg_print("line : '%s'\n" % (line.strip(), ))
            if "Bootloader for ExpressLRS" in line:
                # notify bootloader to start uploading
                s.write(BootloaderInitSeq2)
                s.flush()
                dbg_print("Got into bootloader after: %u attempts\n" % (currAttempt))
                gotBootloader = True
                break
   
    # sanity check! Make sure the bootloader is started
    start = time.time()
    while True:
        try:
            char = s.read(2).decode('utf-8')
        except UnicodeDecodeError:
            continue
        if char == 'CC':
            break
        if ((time.time() - start) > 10):
            msg = "[FAILED] Unable to communicate with bootloader...\n"
            dbg_print(msg)
            raise EnvironmentError(msg)
else:
    dbg_print("\nWe were already in bootloader\n")

# open binary
stream = open(filename, 'rb')
filesize = os.stat(filename).st_size
filechunks = filesize/128

dbg_print("uploading %d bytes...\n" % (filesize,))

s.reset_input_buffer()
s.reset_output_buffer()

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