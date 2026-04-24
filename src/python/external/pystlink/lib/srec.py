import unittest


class SrecException(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return self.msg


class SrecExceptionWrongLength(Exception):
    def __init__(self):
        self.msg = 'Wrong length of record line'

    def __str__(self):
        return self.msg


class SrecExceptionWrongType(Exception):
    def __init__(self, record_type):
        self.msg = 'Wrong record type (%s)' % record_type

    def __str__(self):
        return self.msg


class SrecExceptionWrongChecksum(Exception):
    def __init__(self, checksum):
        self.msg = 'Wrong record checksum (expected: 0xff, get: 0x%02x)' % checksum

    def __str__(self):
        return self.msg


class Srec():
    ADDR_SIZE = {
        'S0': 2,
        'S1': 2,
        'S2': 3,
        'S3': 4,
        'S5': 2,
        'S7': 4,
        'S8': 3,
        'S9': 2,
    }

    def __init__(self):
        self._buffers = []
        self._buffer_addr = 0
        self._buffer_data = None
        self.header = None

    def encode_record(self, srec):
        srec = srec.strip()
        # validate: length of file, start with S, length minimum 6 and even
        if len(srec) < 10 or len(srec) % 2:
            raise SrecExceptionWrongLength()
        record = srec[:2]
        if record not in Srec.ADDR_SIZE:
            raise SrecExceptionWrongType(record)
        data = []
        srec = srec[2:]
        while srec:
            data.append(int(srec[:2], 16))
            srec = srec[2:]
        # validate checksum + remove checksum byte
        checksum = 0x00
        for i in data:
            checksum += i
        checksum &= 0xff
        if checksum != 0xff:
            raise SrecExceptionWrongChecksum(checksum)
        data = data[:-1]
        # validate length + remove length byte
        if data[0] != len(data):
            raise SrecExceptionWrongLength()
        data = data[1:]
        # read address + remove address bytes
        addr_size = Srec.ADDR_SIZE[record]
        addr = 0
        while addr_size:
            addr <<= 8
            addr |= data[0]
            data = data[1:]
            addr_size -= 1
        return record, addr, data

    def process_record(self, srec):
        record, addr, data = self.encode_record(srec)
        if record == 'S0':
            self.header = data
        elif record in ('S1', 'S2', 'S3') and data:
            if self._buffer_addr is None:
                self._buffer_addr = addr
                self._buffer_data = data
            elif self._buffer_addr + len(self._buffer_data) == addr:
                self._buffer_data += data
            elif self._buffer_addr + len(self._buffer_data) != addr:
                self._buffers.append((self._buffer_addr, self._buffer_data))
                self._buffer_addr = addr
                self._buffer_data = data

    def encode_lines(self, srec_lines):
        self._buffers = []
        self._buffer_addr = None
        self._buffer_data = None
        self.header = None
        for srec in srec_lines:
            self.process_record(srec)
        if self._buffer_addr is not None:
            self._buffers.append((self._buffer_addr, self._buffer_data))
        # return self._buffers

    @property
    def buffers(self):
        return self._buffers

    def encode_file(self, filename):
        with open(filename) as srec_file:
            self.encode_lines(srec_file)


class TestSrec(unittest.TestCase):

    def setUp(self):
        self.srec = Srec()

    def testEncodeSrecVeryShortLine1(self):
        with self.assertRaises(SrecExceptionWrongLength):
            self.srec.encode_record('S')

    def testEncodeSrecVeryShortLine3(self):
        with self.assertRaises(SrecExceptionWrongLength):
            self.srec.encode_record('S000')

    def testEncodeSrecLinesWrongRecord(self):
        with self.assertRaises(SrecExceptionWrongType):
            self.srec.encode_record('abcdefghij')

    def testEncodeSrecLinesWrongRecordType(self):
        with self.assertRaises(SrecExceptionWrongType):
            self.srec.encode_record('S600000000')

    def testEncodeSrecWrongChecksum(self):
        with self.assertRaises(SrecExceptionWrongChecksum):
            self.srec.encode_record('S000000000')

    def testEncodeSrecWrongChecksum2(self):
        with self.assertRaises(SrecExceptionWrongChecksum):
            self.srec.encode_record('S012345678901234567890')

    def testEncodeSrecShortLine(self):
        with self.assertRaises(SrecExceptionWrongLength):
            self.srec.encode_record('S0000000ff')

    def testEncodeSrecShortLine2(self):
        with self.assertRaises(SrecExceptionWrongLength):
            self.srec.encode_record('S0020000fd')

    def testEncodeSrecLongLine(self):
        with self.assertRaises(SrecExceptionWrongLength):
            self.srec.encode_record('S0040000fb')

    def testEncodeSrecEmptyHeader(self):
        ret = self.srec.encode_record('S0030000FC')
        self.assertEqual(ret, ('S0', 0x0000, []))

    def testEncodeSrecHeader(self):
        ret = self.srec.encode_record('S0060000766C6BAC')
        self.assertEqual(ret, ('S0', 0x0000, [0x76, 0x6c, 0x6b]))

    def testEncodeSrecDataAddr16(self):
        ret = self.srec.encode_record('S1060000766C6BAC')
        self.assertEqual(ret, ('S1', 0x0000, [0x76, 0x6c, 0x6b]))

    def testEncodeSrecDataAddr24(self):
        ret = self.srec.encode_record('S207000000766C6BAB')
        self.assertEqual(ret, ('S2', 0x000000, [0x76, 0x6c, 0x6b]))

    def testEncodeSrecDataAddr32(self):
        ret = self.srec.encode_record('S30800000000766C6BAA')
        self.assertEqual(ret, ('S3', 0x00000000, [0x76, 0x6c, 0x6b]))

    def testEncodeLines1Buffer(self):
        ret = self.srec.encode_lines([
            'S30900000000112233444c',
            'S309000000045566778838',
            'S309000000089ABCDEF0CA',
        ])
        self.assertEqual(ret, [(0x00000000, [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x9a, 0xbc, 0xde, 0xf0])])

    def testEncodeLines2Buffer(self):
        ret = self.srec.encode_lines([
            'S30900000000112233444c',
            'S309000000045566778838',
            'S309000008009ABCDEF0CA',
        ])
        self.assertEqual(ret, [(0x00000000, [0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88]), (2048, [0x9a, 0xbc, 0xde, 0xf0])])


if __name__ == '__main__':
    unittest.main()
