import serial, time, sys, re
import argparse
import serials_find
import bootloader
from BFinitPassthrough import *

SCRIPT_DEBUG = 0

def send_model_command(args):
    dbg_print("======== SEND SET MODEL COMMAND ========")
    s = serial.Serial(port=args.port, baudrate=args.baud,
        bytesize=8, parity='N', stopbits=1,
        timeout=1, xonxoff=0, rtscts=0)
    TelemSeq = bootloader.get_model_seq(args.model)
    s.write(TelemSeq)
    s.flush()
    time.sleep(.5)
    s.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Initialize BetaFlight passthrough and send the 'enter bind mode' command")
    parser.add_argument(
        'model', metavar='int', nargs=1, type=int,
        help='The model match number to set')
    parser.add_argument("-b", "--baud", type=int, default=420000,
        help="Baud rate for passthrough communication")
    parser.add_argument("-p", "--port", type=str,
        help="Override serial port autodetection and use PORT")
    args = parser.parse_args()

    if (args.port == None):
        args.port = serials_find.get_serial_port()

    try:
        bf_passthrough_init(args.port, args.baud)
    except PassthroughEnabled as err:
        dbg_print(str(err))

    send_model_command(args)
