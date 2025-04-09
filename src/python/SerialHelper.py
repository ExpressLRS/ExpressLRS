import time

encoding = "utf-8"

class SerialHelper:
    def __init__(self, serial, timeout=2, delimiters=["\n", "CCC"]):
        self.serial = serial
        self.timeout = timeout
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

    def write(self, data):
        serial = self.serial
        data = self.encode(data)
        cnt = serial.write(data)
        serial.flush()

    def write_str(self, data, new_line=True):
        if (new_line):
            self.write(data+'\r\n')
        else:
            self.write(data)


    def __convert_to_str(self, data):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            return ""
