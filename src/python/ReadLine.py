import time

# Helper class to return serial output on a list of custom delimiters
# Will return no matter what after timeout has passed.
class ReadLine:
    def __init__(self, serial, timeout=2, delimiters=["\n", "CCC"]):
        self.serial = serial
        self.timeout = timeout
        self.clear()
        self.set_delimiters(delimiters)

    def clear(self):
        self.buf = bytearray()

    def set_timeout(self, timeout):
        self.timeout = timeout

    def set_delimiters(self, delimiters):
        self.delimiters = [
            bytes(d, "utf-8") if type(d) == str else d for d in delimiters]

    def read_line(self, timeout=None):
        if timeout is None or timeout <= 0.:
            timeout = self.timeout
        for delimiter in self.delimiters:
            i = self.buf.find(delimiter)
            if i >= 0:
                offset = i+len(delimiter)
                r = self.buf[:offset]
                self.buf = self.buf[offset:]
                return r

        start = time.time()
        while ((time.time() - start) < timeout):
            i = max(0, min(2048, self.serial.in_waiting))
            data = self.serial.read(i)
            if not data:
                continue
            for delimiter in self.delimiters:
                i = bytearray(self.buf + data).find(delimiter)
                if i >= 0:
                    offset = i+len(delimiter)
                    r = self.buf + data[:offset]
                    self.buf = bytearray(data[offset:])
                    return r
            # No match
            self.buf.extend(data)

        # Timeout, return buffer and reset it
        data = self.buf
        self.buf = bytearray()
        return data
