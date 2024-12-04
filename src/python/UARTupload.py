import serial
from external.xmodem import XMODEM
import time
import sys
import logging
import os
import serials_find
import BFinitPassthrough
import SerialHelper
import re
import bootloader
from query_yes_no import query_yes_no
from elrs_helpers import ElrsUploadResult

BAUDRATE_DEFAULT = 420000

def dbg_print(line=''):
    print(line, flush=True)
    return


def uart_upload(port, filename, baudrate, ghst=False, ignore_incorrect_target=False, key=None, target="", accept=None) -> int:
    SCRIPT_DEBUG = False
    half_duplex = False

    dbg_print("=================== FIRMWARE UPLOAD ===================\n")
    dbg_print("  Bin file '%s'\n" % filename)
    dbg_print("  Port %s @ %s\n" % (port, baudrate))

    logging.basicConfig(level=logging.ERROR)

    if ghst:
        BootloaderInitSeq1 = bootloader.get_init_seq('GHST', key)
        half_duplex = True
        dbg_print("  Using GHST (half duplex)!\n")
    else:
        BootloaderInitSeq1 = bootloader.get_init_seq('CRSF', key)
        dbg_print("  Using CRSF (full duplex)!\n")
    BootloaderInitSeq2 = bytes([0x62,0x62,0x62,0x62,0x62,0x62])

    if not os.path.exists(filename):
        msg = "[FAILED] file '%s' does not exist\n" % filename
        dbg_print(msg)
        return ElrsUploadResult.ErrorGeneral

    s = serial.Serial(port=port, baudrate=baudrate,
        bytesize=8, parity='N', stopbits=1,
        timeout=5, inter_byte_timeout=None, xonxoff=0, rtscts=0)

    rl = SerialHelper.SerialHelper(s, 2., ["CCC"], half_duplex)

    # Check if bootloader *and* passthrough is already active
    gotBootloader = 'CCC' in rl.read_line()

    if not gotBootloader:
        s.close()

        # Init Betaflight passthrough
        try:
            BFinitPassthrough.bf_passthrough_init(port, baudrate, half_duplex)
        except BFinitPassthrough.PassthroughEnabled as info:
            dbg_print("  Warning: '%s'\n" % info)
        except BFinitPassthrough.PassthroughFailed as failed:
            SystemExit(failed)

        # Prepare to upload
        s = serial.Serial(port=port, baudrate=baudrate,
            bytesize=8, parity='N', stopbits=1,
            timeout=1, xonxoff=0, rtscts=0)
        rl.set_serial(s)
        rl.clear()

        # Check again if we're in the bootloader now that passthrough is setup;
        #   Note: This is for button-method flashing
        gotBootloader = 'CCC' in rl.read_line()

        # Init bootloader
        if not gotBootloader:
            # legacy bootloader requires a 500ms delay
            delay_seq2 = .5

            rl.set_timeout(2.)
            rl.set_delimiters(["\n", "CCC"])

            currAttempt = 0
            dbg_print("\nAttempting to reboot into bootloader...\n")

            while gotBootloader == False:
                currAttempt += 1
                if 10 < currAttempt:
                    msg = "[FAILED] to get to BL in reasonable time\n"
                    dbg_print(msg)
                    return ElrsUploadResult.ErrorGeneral

                if 5 < currAttempt:
                    # Enable debug logs after 5 retries
                    SCRIPT_DEBUG = True

                dbg_print("[%1u] retry...\n" % currAttempt)

                # clear RX buffer before continuing
                rl.clear()
                # request reboot
                rl.write(BootloaderInitSeq1)

                start = time.time()
                while ((time.time() - start) < 2.):
                    line = rl.read_line().strip()
                    if not line:
                        # timeout
                        continue

                    if SCRIPT_DEBUG and line:
                        dbg_print(" **DBG : '%s'\n" % line)

                    if "BL_TYPE" in line:
                        bl_type = line[8:].strip()
                        dbg_print("    Bootloader type found : '%s'\n" % bl_type)
                        # Newer bootloaders do not require delay but keep
                        # a 100ms just in case
                        delay_seq2 = .1
                        continue

                    versionMatch = re.search('=== (v.*) ===', line, re.IGNORECASE)
                    if versionMatch:
                        bl_version = versionMatch.group(1)
                        dbg_print("    Bootloader version found : '%s'\n" % bl_version)

                    elif "hold down button" in line:
                        time.sleep(delay_seq2)
                        rl.write(BootloaderInitSeq2)
                        gotBootloader = True
                        break

                    elif "CCC" in line:
                        # Button method has been used so we're not catching the header;
                        #  let's jump right to the sanity check if we're getting enough C's
                        gotBootloader = True
                        break

                    elif "_RX" in line:
                        if line != target and line != accept and not ignore_incorrect_target:
                            if query_yes_no("\n\n\nWrong target selected! your RX is '%s', trying to flash '%s', continue? Y/N\n" % (line, target)):
                                ignore_incorrect_target = True
                                continue
                            else:
                                dbg_print("Wrong target selected your RX is '%s', trying to flash '%s'" % (line, target))
                                return ElrsUploadResult.ErrorMismatch
                        elif target != "":
                            dbg_print("Verified RX target '%s'" % target)

            dbg_print("    Got into bootloader after: %u attempts\n" % currAttempt)

            # sanity check! Make sure the bootloader is started
            dbg_print("Wait sync...")
            rl.set_delimiters(["CCC"])
            if "CCC" not in rl.read_line(15.):
                msg = "[FAILED] Unable to communicate with bootloader...\n"
                dbg_print(msg)
                return ElrsUploadResult.ErrorGeneral
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
    filechunks = filesize / 128

    dbg_print("\nuploading %d bytes...\n" % filesize)

    def StatusCallback(total_packets, success_count, error_count):
        #sys.stdout.flush()
        if (total_packets % 10 == 0):
            dbg = str(round((total_packets / filechunks) * 100)) + "%"
            if (error_count > 0):
                dbg += ", err: " + str(error_count)
            dbg_print(dbg)

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
        return ElrsUploadResult.Success

    dbg_print("[FAILED] Upload failed!\n\n")
    return ElrsUploadResult.ErrorGeneral


def on_upload(source, target, env):
    envkey = None
    ghst = False
    firmware_path = str(source[0])
    upload_force = target[0].name == 'uploadforce' # 'in' operator doesn't work on Alias list

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
            elif "BL_KEY=" in flag:
                envkey = flag.split("=")[1]

    try:
        returncode = uart_upload(upload_port, firmware_path, upload_speed, ghst, upload_force, key=envkey, target=re.sub("_VIA_.*", "", env['PIOENV'].upper()))
    except Exception as e:
        dbg_print("{0}\n".format(e))
        return ElrsUploadResult.ErrorGeneral
    return returncode


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

    returncode = uart_upload(port, filename, baudrate, ignore_incorrect_target=False)
    exit(returncode)
