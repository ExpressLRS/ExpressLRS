import sys
import serial

s = serial.Serial(port=sys.argv[1], baudrate=4800,
    bytesize=8, parity='N', stopbits=1,
    timeout=1, xonxoff=0, rtscts=0, write_timeout=0.1)

input = True
output = True
debug = False
if len(sys.argv) > 2:
    if 'i' in sys.argv[2]:
        output = False
    elif 'o' in sys.argv[2]:
        input = False
    if 'd' in sys.argv[2]:
        debug = True

prevb = 31
i = 32
fail = False
l = 0
while(True):
    if output:
        try:
            if s.out_waiting < 10:
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                s.write(i.to_bytes(1, 'little'))
                i = (i + 1) % 256
                if i == 127: i = 32
        except serial.SerialTimeoutException:
            None

    if input and s.in_waiting > 0:
        bs = s.read()
        for b in bs:
            expect = (prevb + 1) % 256
            if expect == 127: expect = 32
            if fail:
                if b == expect:
                    fail = False
                    prevb = b
            elif b != expect and b != prevb:
                print(f'fail got {b} expected {expect}')
                fail = True
                prevb = 32
            else:
                prevb = b

    if debug and s.in_waiting > 0:
        bs = s.read()
        print(bs.decode('cp1251'), end='')
        l = l + len(bs)
        if l >= 64:
            print('')
            l = 0
