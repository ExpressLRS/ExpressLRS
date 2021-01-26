import serial, time, sys, re
from xmodem import XMODEM
import serials_find
import SerialHelper

SCRIPT_DEBUG = 0


class PassthroughEnabled(Exception):
    pass

class PassthroughFailed(Exception):
    pass


def dbg_print(line=''):
    sys.stdout.write(line + '\n')
    sys.stdout.flush()


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
    rl.write("#\r\n", half_duplex)
    start = rl.read_line(2.).strip()
    #dbg_print("BF INIT: '%s'" % start.replace("\r", ""))
    if "CCC" in start:
        raise PassthroughEnabled("Passthrough already enabled and bootloader active")
    elif not start or not start.endswith("#"):
        raise PassthroughEnabled("No CLI available. Already in passthrough mode?")

    SerialRXindex = ""

    dbg_print("\nAttempting to detect FC UART configuration...")

    rl.set_delimiters(["\n"])
    rl.clear()
    rl.write("serial\r\n")

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
    rl.write(cmd + '\n')
    time.sleep(.2)
    s.close()

    dbg_print("======== PASSTHROUGH DONE ========")


if __name__ == '__main__':
    try:
        requestedBaudrate = int(sys.argv[1])
    except:
        requestedBaudrate = 420000
    port = serials_find.get_serial_port()
    try:
        bf_passthrough_init(port, requestedBaudrate)
    except:
        dbg_print("Assuming that FC is in Passthrough already.")
