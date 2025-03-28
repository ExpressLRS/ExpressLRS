import time, serial

encoding = "utf-8"

class SerialHelper:
    def __init__(self, serial, timeout=2, delimiters=["\n", "CCC"], half_duplex=False):
        self.serial = serial
        self.timeout = timeout
        self.half_duplex = half_duplex
        self.clear()
        self.set_delimiters(delimiters)

    def encode(self, data):
        if type(data) == str:
            return data.encode(encoding)
        return data

    def clear(self):
        self.serial.reset_input_buffer()
        self.buf = bytearray()

    def set_serial(self, serial):
        self.serial = serial

    def set_timeout(self, timeout):
        self.timeout = timeout

    def set_delimiters(self, delimiters):
        self.delimiters = [
            bytes(d, encoding) if type(d) == str else d for d in delimiters]

    def read_line(self, timeout=None):
        if timeout is None or timeout <= 0.:
            timeout = self.timeout
        buf = self.buf
        def has_delimiter():
            for d in self.delimiters:
                if d in buf:
                    return True

        start = time.time()
        while not has_delimiter() and ((time.time() - start) < timeout):
            i = max(0, min(2048, self.serial.in_waiting))
            data = self.serial.read(i)
            if not data:
                continue

            buf.extend(data)

        for delimiter in self.delimiters:
            i = buf.find(delimiter)
            if i >= 0:
                offset = i+len(delimiter)
                r = buf[:offset]
                self.buf = buf[offset:]
                return self.__convert_to_str(r)

        # No match

        # Timeout, reset buffer and return empty string
        #print("TIMEOUT! Got:\n>>>>>>\n{}\n<<<<<<\n".format(buf))
        self.buf = bytearray()
        return ""

    def write(self, data, half_duplex=None):
        if half_duplex is None:
            half_duplex = self.half_duplex
        serial = self.serial
        data = self.encode(data)
        cnt = serial.write(data)
        serial.flush()
        if half_duplex:
            # Clean RX buffer in case of half duplex
            #   All written data is read into RX buffer
            serial.read(cnt)

    def write_str(self, data, half_duplex=None, new_line=True):
        if (new_line):
            self.write(data+'\r\n', half_duplex)
        else:
            self.write(data, half_duplex)


    def __convert_to_str(self, data):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            return ""
