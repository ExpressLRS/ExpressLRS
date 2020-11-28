import serial
from xmodem import XMODEM
import time
import sys
import logging
import os
import serials_find
import BFinitPassthrough


BAUDRATE_DEFAULT = 420000


def dbg_print(line=''):
    sys.stdout.write(line)
    sys.stdout.flush()
    return


def uart_upload(port, filename, baudrate):
    dbg_print("=================== FIRMWARE UPLOAD ===================\n")
    dbg_print("  Bin file '%s'\n" % filename)
    dbg_print("  Port %s @ %s\n" % (port, baudrate))

    logging.basicConfig(level=logging.ERROR)

    BootloaderInitSeq1 = bytes([0xEC,0x04,0x32,0x62,0x6c,0x0A])
    BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

    if not os.path.exists(filename):
        msg = "[FAILED] file '%s' does not exist\n" % filename
        dbg_print(msg)
        raise Exception(msg)

    s = serial.Serial(port=port, baudrate=baudrate, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

    # Check if bootloader is already active
    try:
        already_in_bl = 'CC' in s.read(2).decode('utf-8')
    except UnicodeDecodeError:
        already_in_bl = False

    if not already_in_bl:
        s.close()

        # Init Betaflight passthrough
        try:
            BFinitPassthrough.bf_passthrough_init(port, baudrate)
        except BFinitPassthrough.PassthroughEnabled:
            pass

        # Init bootloader next
        s = serial.Serial(port=port, baudrate=baudrate, bytesize=8, parity='N', stopbits=1, timeout=5, xonxoff=0, rtscts=0)

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
                raise Exception(msg)

            # request reboot
            line = ""
            s.write(BootloaderInitSeq1)
            s.flush()
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
                    for idx in range(2):
                        line = s.readline().decode('utf-8')
                        if "BL_TYPE" in line:
                            # do check...
                            dbg_print("line : '%s'\n" % (line, ))
                            bl_ver = line.strip()[8:].strip()
                            dbg_print("Bootloader type : '%s'\n" % (bl_ver, ))
                            break
                    # notify bootloader to start uploading
                    s.write(BootloaderInitSeq2)
                    s.flush()
                    dbg_print("Got into bootloader after: %u attempts\n" % (currAttempt))
                    gotBootloader = True
                    break

        # sanity check! Make sure the bootloader is started
        dbg_print("Wait sync...")
        start = time.time()
        while True:
            try:
                char = s.read(2).decode('utf-8')
            except UnicodeDecodeError:
                continue
            if char == 'CC':
                break
            if ((time.time() - start) > 15):
                msg = "[FAILED] Unable to communicate with bootloader...\n"
                dbg_print(msg)
                raise Exception(msg)
        dbg_print("  ... sync OK\n")
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
        raise Exception('Failed to Upload')


def on_upload(source, target, env):
    firmware_path = str(source[0])

    upload_port = env.get('UPLOAD_PORT', None)
    if upload_port is None:
        upload_port = serials_find.get_serial_port()
    upload_speed = env.get('UPLOAD_SPEED', None)
    if upload_speed is None:
        upload_speed = BAUDRATE_DEFAULT

    uart_upload(upload_port, firmware_path, upload_speed)


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
