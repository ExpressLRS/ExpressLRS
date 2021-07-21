import serial, time, sys, re
import argparse
import serials_find
import SerialHelper
import bootloader
from BFinitPassthrough import *

SCRIPT_DEBUG = 0

def send_bind_command(args):
    dbg_print("======== SEND BIND COMMAND ========")
    s = serial.Serial(port=args.port, baudrate=args.baud,
        bytesize=8, parity='N', stopbits=1,
        timeout=1, xonxoff=0, rtscts=0)
    if args.half_duplex:
        BindInitSeq = bootloader.get_bind_seq('GHST', args.type)
        dbg_print("  * Using half duplex (GHST)")
    else:
        BindInitSeq = bootloader.get_bind_seq('CRSF', args.type)
        dbg_print("  * Using full duplex (CRSF)")
    s.write(BindInitSeq)
    s.flush()
    time.sleep(.5)
    s.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Initialize BetaFlight passthrough and send the 'enter bind mode' command")
    parser.add_argument("-b", "--baud", type=int, default=420000,
        help="Baud rate for passthrough communication")
    parser.add_argument("-p", "--port", type=str,
        help="Override serial port autodetection and use PORT")
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

    send_bind_command(args)
