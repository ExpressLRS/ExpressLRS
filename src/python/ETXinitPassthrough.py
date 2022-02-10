import serial, time, sys
import argparse
import serials_find
import external.streamexpect as streamexpect


def dbg_print(line=''):
    sys.stdout.write(line + '\n')
    sys.stdout.flush()


def etx_passthrough_init(port, requestedBaudrate):
    sys.stdout.flush()
    dbg_print("======== PASSTHROUGH INIT ========")
    dbg_print("  Trying to initialize %s @ %s" % (port, requestedBaudrate))

    s = serial.Serial(port=port, baudrate=requestedBaudrate,
        bytesize=8, parity='N', stopbits=1,
        timeout=1, xonxoff=0, rtscts=0)

    with streamexpect.wrap(s) as rl:
        rl.flush()
        rl.write(b"set pulses 0\n")
        rl.expect_bytes(b"set: ", timeout=1.0)
        rl.expect_bytes(b"> ", timeout=1.0)
        rl.write(b"set rfmod 0 power off\n")
        rl.expect_bytes(b"set: ", timeout=1.0)
        rl.expect_bytes(b"> ", timeout=1.0)
        time.sleep(.5)
        rl.write(b"set rfmod 0 bootpin 1\n")
        rl.expect_bytes(b"set: ", timeout=1.0)
        rl.expect_bytes(b"> ", timeout=1.0)
        time.sleep(.1)
        rl.write(b"set rfmod 0 power on\n")
        rl.expect_bytes(b"set: ", timeout=1.0)
        rl.expect_bytes(b"> ", timeout=1.0)
        time.sleep(.1)
        rl.write(b"set rfmod 0 bootpin 0\n")
        rl.expect_bytes(b"set: ", timeout=1.0)
        rl.expect_bytes(b"> ", timeout=1.0)

        cmd = "serialpassthrough rfmod 0 %s" % requestedBaudrate

        dbg_print("Enabling serial passthrough...")
        dbg_print("  CMD: '%s'" % cmd)
        rl.write(cmd.encode("utf-8"))
        rl.write(b'\n')
        time.sleep(.2)

    s.close()
    dbg_print("======== PASSTHROUGH DONE ========")

def init_passthrough(source, target, env):
    env.AutodetectUploadPort([env])
    port = env['UPLOAD_PORT']
    etx_passthrough_init(port, env['UPLOAD_SPEED'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Initialize EdgeTX passthrough to internal module")
    parser.add_argument("-b", "--baud", type=int, default=460800,
        help="Baud rate for passthrough communication")
    parser.add_argument("-p", "--port", type=str,
        help="Override serial port autodetection and use PORT")
    args = parser.parse_args()

    if (args.port == None):
        args.port = serials_find.get_serial_port()

    etx_passthrough_init(args.port, args.baud)
