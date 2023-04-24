import serial, time, sys
import external.streamexpect as streamexpect


def waitForOk(strm):
    strm.expect_bytes(b'OK\r\n')

if __name__ == '__main__':
    s = serial.Serial(port='COM3', baudrate=115200,
        bytesize=8, parity='N', stopbits=1,
        timeout=0, xonxoff=0, rtscts=0)

    with streamexpect.wrap(s) as strm:
        strm.flush()
        strm.write(b'V\r\n')
        ver = strm.expect_regex(br'^RFPowerMeterv2 ([0-9\.]+)\r\n').groups[0].decode('ascii')
        waitForOk(strm)
        print(f'Power Meter found, version {ver}')

        strm.write(b'F6\r\n')
        ver = strm.expect_bytes(b'2400\r\n')
        waitForOk(strm)
        print(f'2400MHz selected')

        while True:
            strm.write(b'E\r\n')
            dbm = strm.expect_regex(br'^([0-9\.\-]+)\r\n').groups[0].decode('ascii')
            print(dbm)
            waitForOk(strm)
            
            time.sleep(10.0)