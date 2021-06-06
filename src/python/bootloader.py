
INIT_SEQ = {
    "CRSF": [0xEC,0x04,0x32,ord('b'),ord('l')],
    "GHST": [0x89,0x04,0x32,ord('b'),ord('l')],
}

BIND_SEQ = {
    "CRSF": [0xEC,0x04,0x32,ord('b'),ord('d')],
    "GHST": [0x89,0x04,0x32,ord('b'),ord('d')],
}

def calc_crc8(payload, poly=0xD5):
    crc = 0
    for data in payload:
        crc ^= data
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ poly
            else:
                crc = crc << 1
    return crc & 0xFF

def get_init_seq(module, key=None):
    payload = list(INIT_SEQ.get(module, []))
    if payload:
        if key:
            if type(key) == str:
                key = [ord(x) for x in key]
            payload += key
            payload[1] += len(key)
        payload += [calc_crc8(payload[2:])]
    return bytes(payload)

def get_bind_seq(module, key=None):
    payload = list(BIND_SEQ.get(module, []))
    if payload:
        if key:
            if type(key) == str:
                key = [ord(x) for x in key]
            payload += key
            payload[1] += len(key)
        payload += [calc_crc8(payload[2:])]
    return bytes(payload)

if __name__ == '__main__':
    print("CRC: %s" % get_init_seq('CRSF', [ord(x) for x in 'R9MM']))
    print("CRC: %s" % get_init_seq('CRSF', 'R9MM'))
