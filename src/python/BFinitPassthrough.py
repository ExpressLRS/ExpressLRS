import serial, time, sys, re
import argparse
import serials_find
import SerialHelper
import bootloader
from query_yes_no import query_yes_no

SCRIPT_DEBUG = 0


class PassthroughEnabled(Exception):
    pass

class PassthroughFailed(Exception):
    pass

class WrongTargetSelected(Exception):
    pass

def dbg_print(line=''):
    sys.stdout.write(line + '\n')
    sys.stdout.flush()


def _validate_serialrx(rl, config, expected):
    found = False
    if type(expected) == str:
        expected = [expected]
    rl.set_delimiters(["# "])
    rl.clear()
    rl.write_str("get serialrx_%s" % config)
    line = rl.read_line(1.).strip()
    for key in expected:
        key = " = %s" % key
        if key in line:
            found = True
            break
    return found


def bf_passthrough_init(port, requestedBaudrate, half_duplex=False):
    debug = SCRIPT_DEBUG

    sys.stdout.flush()
    dbg_print("======== PASSTHROUGH INIT ========")
    dbg_print("  Trying to initialize %s @ %s" % (port, requestedBaudrate))

    s = serial.Serial(port=port, baudrate=115200,
        bytesize=8, parity='N', stopbits=1,
        timeout=1, xonxoff=0, rtscts=0)

    rl = SerialHelper.SerialHelper(s, 3., ['CCC', "# "])
    rl.clear()
    # Send start command '#'
    rl.write_str("#", half_duplex)
    start = rl.read_line(2.).strip()
    #dbg_print("BF INIT: '%s'" % start.replace("\r", ""))
    if "CCC" in start:
        raise PassthroughEnabled("Passthrough already enabled and bootloader active")
    elif not start or not start.endswith("#"):
        raise PassthroughEnabled("No CLI available. Already in passthrough mode?, If this fails reboot FC and try again!")

    serial_check = []
    if not _validate_serialrx(rl, "provider", [["CRSF", "ELRS"], "GHST"][half_duplex]):
        serial_check.append("serialrx_provider != CRSF")
    if not _validate_serialrx(rl, "inverted", "OFF"):
        serial_check.append("serialrx_inverted != OFF")
    if not _validate_serialrx(rl, "halfduplex", ["OFF", "AUTO"]):
        serial_check.append("serialrx_halfduplex != OFF/AUTO")

    if serial_check:
        error = "\n\n [ERROR] Invalid serial RX configuration detected:\n"
        for err in serial_check:
            error += "    !!! %s !!!\n" % err
        error += "\n    Please change the configuration and try again!\n"
        dbg_print(error)
        raise PassthroughFailed(error)

    SerialRXindex = ""

    dbg_print("\nAttempting to detect FC UART configuration...")

    rl.set_delimiters(["\n"])
    rl.clear()
    rl.write_str("serial")

    while True:
        line = rl.read_line().strip()
        #print("FC: '%s'" % line)
        if not line or "#" in line:
            break

        if line.startswith("serial"):
            if debug:
                dbg_print("  '%s'" % line)
            config = re.search('serial ([0-9]+) ([0-9]+) ', line)
            if config and config.group(2) == "64":
                dbg_print("    ** Serial RX config detected: '%s'" % line)
                SerialRXindex = config.group(1)
                if not debug:
                    break

    if not SerialRXindex:
        raise PassthroughFailed("!!! RX Serial not found !!!!\n  Check configuration and try again...")

    cmd = "serialpassthrough %s %s" % (SerialRXindex, requestedBaudrate, )

    dbg_print("Enabling serial passthrough...")
    dbg_print("  CMD: '%s'" % cmd)
    rl.write_str(cmd)
    time.sleep(.2)
    s.close()
    dbg_print("======== PASSTHROUGH DONE ========")


def reset_to_bootloader(args):
    dbg_print("======== RESET TO BOOTLOADER ========")
    s = serial.Serial(port=args.port, baudrate=args.baud,
        bytesize=8, parity='N', stopbits=1,
        timeout=1, xonxoff=0, rtscts=0)
    rl = SerialHelper.SerialHelper(s, 3.)
    rl.clear()
    if args.half_duplex:
        BootloaderInitSeq = bootloader.get_init_seq('GHST', args.type)
        dbg_print("  * Using half duplex (GHST)")
    else:
        BootloaderInitSeq = bootloader.get_init_seq('CRSF', args.type)
        dbg_print("  * Using full duplex (CRSF)")
        #this is the training sequ for the ROM bootloader, we send it here so it doesn't auto-neg to the wrong baudrate by the BootloaderInitSeq that we send to reset ELRS
        rl.write(b'\x07\x07\x12\x20' + 32 * b'\x55')
        time.sleep(0.2)
    rl.write(BootloaderInitSeq)
    s.flush()
    rx_target = rl.read_line().strip()
    flash_target = re.sub("_VIA_.*", "", args.target.upper())
    if rx_target == "":
        dbg_print("Cannot detect RX target, blindly flashing!")
    elif rx_target != flash_target:
        if query_yes_no("\n\n\nWrong target selected! your RX is '%s', trying to flash '%s', continue? Y/N\n" % (rx_target, flash_target)):
            dbg_print("Ok, flashing anyway!")
        else:
            raise WrongTargetSelected("Wrong target selected your RX is '%s', trying to flash '%s'" % (rx_target, flash_target))
    elif flash_target != "":
        dbg_print("Verified RX target '%s'" % (flash_target))
    time.sleep(.5)
    s.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Initialize BetaFlight passthrough and optionally send a reboot comamnd sequence")
    parser.add_argument("-b", "--baud", type=int, default=420000,
        help="Baud rate for passthrough communication")
    parser.add_argument("-p", "--port", type=str,
        help="Override serial port autodetection and use PORT")
    parser.add_argument("-r", "--target", type=str,
        help="The target firmware that is going to be uploaded")
    parser.add_argument("-nr", "--no-reset", action="store_false",
        dest="reset_to_bl", help="Do not send reset_to_bootloader command sequence")
    parser.add_argument("-hd", "--half-duplex", action="store_true",
        dest="half_duplex", help="Use half duplex mode")
    parser.add_argument("-t", "--type", type=str, default="ESP82",
        help="Defines flash target type which is sent to target in reboot command")
    args = parser.parse_args()

    if (args.port == None):
        args.port = serials_find.get_serial_port()

    try:
        bf_passthrough_init(args.port, args.baud)
    except PassthroughEnabled as err:
        dbg_print(str(err))

    if args.reset_to_bl:
        try:
            reset_to_bootloader(args)
        except WrongTargetSelected as err:
            dbg_print(str(err))
            exit(-1)
