import serial
from xmodem import XMODEM
import time
import sys
import logging
import os
import serials_find
import BFinitPassthrough
import ReadLine
import re

SCRIPT_DEBUG = 0
BAUDRATE_DEFAULT = 420000


def dbg_print(line=''):
    sys.stdout.write(line)
    sys.stdout.flush()
    return


def uart_upload(port, filename, baudrate, ghst=False):
    half_duplex = False

    dbg_print("=================== FIRMWARE UPLOAD ===================\n")
    dbg_print("  Bin file '%s'\n" % filename)
    dbg_print("  Port %s @ %s\n" % (port, baudrate))

    logging.basicConfig(level=logging.ERROR)

    BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A]) # CRSF
    if ghst:
        BootloaderInitSeq1 = bytes([0x89,0x04,0x32,0x62,0x6c,0x0A]) # GHST
        half_duplex = True
        dbg_print("  Using GHST (half duplex)!\n")
    BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

    if not os.path.exists(filename):
        msg = "[FAILED] file '%s' does not exist\n" % filename
        dbg_print(msg)
        raise Exception(msg)

    s = serial.Serial(port=port, baudrate=baudrate, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

    # Check if bootloader *and* passthrough is already active
    try:
        gotBootloader = 'CCC' in s.read(3).decode('utf-8')
    except UnicodeDecodeError:
        gotBootloader = False

    if not gotBootloader:
        s.close()

        # Init Betaflight passthrough
        try:
            BFinitPassthrough.bf_passthrough_init(port, baudrate, half_duplex)
        except BFinitPassthrough.PassthroughEnabled as bf_err:
            dbg_print("  FC Init error: '%s'" % bf_err)

        # Init bootloader next
        s = serial.Serial(port=port, baudrate=baudrate, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

        # Check again if we're in the bootloader now that passthrough is setup; This is for button-method flashing
        try:
            gotBootloader = 'CCC' in s.read(3).decode('utf-8')
        except UnicodeDecodeError:
            pass

        # Init bootloader
        if gotBootloader == False:
            # legacy bootloader requires a 500ms delay
            delay_seq2 = .5

            s.timeout = .5
            s.write_timeout = .5

            retryTimeout = 2.0
            rl = ReadLine.ReadLine(s, retryTimeout)

            currAttempt = 0
            dbg_print("\nAttempting to reboot into bootloader...\n")

            while gotBootloader == False:

                currAttempt += 1
                if 10 < currAttempt:
                    msg = "[FAILED] to get to BL in reasonable time\n"
                    dbg_print(msg)
                    raise Exception(msg)
                    
                if 6 < currAttempt:
                    # Enable debug
                    global SCRIPT_DEBUG
                    SCRIPT_DEBUG = 1

                dbg_print("[%1u] retry...\n" % currAttempt)

                # clear RX buffer before continuing
                s.reset_input_buffer()

                # request reboot
                cnt = s.write(BootloaderInitSeq1)
                s.flush()
                if half_duplex:
                   s.read(cnt)

                start = time.time()
                while ((time.time() - start) < retryTimeout):
                    try:
                        line = rl.read_line().decode('utf-8')
                    except UnicodeDecodeError:
                        continue
                    if SCRIPT_DEBUG and line:
                        dbg_print(" **DBG : '%s'\n" % line.strip())

                    if "BL_TYPE" in line:
                        bl_type = line.strip()[8:].strip()
                        dbg_print("    Bootloader type found : '%s'\n" % bl_type)
                        # Newer bootloaders do not require delay but keep
                        # a 100ms just in case
                        delay_seq2 = .1
                        continue

                    versionMatch = re.search('=== (v.*) ===', line, re.IGNORECASE)
                    if versionMatch:
                        bl_version = versionMatch.group(1)
                        dbg_print("    Bootloader version found : '%s'\n" % bl_version)

                    elif "hold down button" in line.lower():
                        time.sleep(delay_seq2)
                        cnt = s.write(BootloaderInitSeq2)
                        s.flush()
                        if half_duplex:
                            s.read(cnt)
                        gotBootloader = True
                        break

                    elif "CCC" in line:
                        # Button method has been used so we're not catching the header;
                        #  let's jump right to the sanity check if we're getting enough C's
                        gotBootloader = True
                        break

            dbg_print("    Got into bootloader after: %u attempts\n" % currAttempt)

            # change timeout to 30sec
            s.timeout = 30.
            s.write_timeout = 5.

            # sanity check! Make sure the bootloader is started
            dbg_print("Wait sync...")
            start = time.time()
            while True:
                try:
                    char = s.read(3).decode('utf-8')
                except UnicodeDecodeError:
                    continue
                if SCRIPT_DEBUG and char:
                    dbg_print(" **DBG : '%s'\n" % char)
                if char == 'CCC':
                    break
                if ((time.time() - start) > 15):
                    msg = "[FAILED] Unable to communicate with bootloader...\n"
                    dbg_print(msg)
                    raise Exception(msg)
            dbg_print(" sync OK\n")
        else:
            dbg_print("\nWe were already in bootloader\n")
    else:
        dbg_print("\nWe were already in bootloader\n")

    # change timeout to 5sec
    s.timeout = 5.
    s.write_timeout = .3

    # open binary
    stream = open(filename, 'rb')
    filesize = os.stat(filename).st_size
    filechunks = filesize/128

    dbg_print("\nuploading %d bytes...\n" % filesize)

    def StatusCallback(total_packets, success_count, error_count):
        #sys.stdout.flush()
        if (total_packets % 10 == 0):
            if (error_count > 0):
                dbg_print(str(round((total_packets/filechunks)*100)) + "% err: " + str(error_count) + "\n")
            else:
                dbg_print(str(round((total_packets/filechunks)*100)) + "%\n")

    def getc(size, timeout=3):
        return s.read(size) or None

    def putc(data, timeout=3):
        cnt = s.write(data)
        if half_duplex:
            s.flush()
            # Clean RX buffer in case of half duplex
            #   All written data is read into RX buffer
            s.read(cnt)
        return cnt

    s.reset_input_buffer()

    modem = XMODEM(getc, putc, mode='xmodem')
    #modem.log.setLevel(logging.DEBUG)
    status = modem.send(stream, retry=10, callback=StatusCallback)

    s.close()
    stream.close()

    if (status):
        dbg_print("Success!!!!\n\n")
    else:
        dbg_print("[FAILED] Upload failed!\n\n")
        raise Exception('Failed to Upload')


def on_upload(source, target, env):
    ghst = False
    firmware_path = str(source[0])

    upload_port = env.get('UPLOAD_PORT', None)
    if upload_port is None:
        upload_port = serials_find.get_serial_port()
    upload_speed = env.get('UPLOAD_SPEED', None)
    if upload_speed is None:
        upload_speed = BAUDRATE_DEFAULT

    upload_flags = env.get('UPLOAD_FLAGS', [])
    for line in upload_flags:
        flags = line.split()
        for flag in flags:
            if "GHST=" in flag:
                ghst = eval(flag.split("=")[1])

    uart_upload(upload_port, firmware_path, upload_speed, ghst)


if __name__ == '__main__':
    filename = 'firmware.bin'
    baudrate = BAUDRATE_DEFAULT
    try:
        filename = sys.argv[1]
    except IndexError:
        dbg_print("Filename not provided, going to use default firmware.bin")

    if 2 < len(sys.argv):
        port = sys.argv[2]
    else:
        port = serials_find.get_serial_port()

    if 3 < len(sys.argv):
        baudrate = sys.argv[3]

    uart_upload(port, filename, baudrate)
