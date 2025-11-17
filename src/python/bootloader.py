
INIT_SEQ = [0xEC,0x04,0x32,ord('b'),ord('l')]

BIND_SEQ = [0xEC,0x04,0x32,ord('b'),ord('d')]

MODEL_SEQ = [0xEC,0x04,0x32,ord('m'),ord('m')]

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

def get_telemetry_seq(seq, key=None):
    payload = list(seq)
    if payload:
        if key:
            if type(key) == str:
                key = [ord(x) for x in key]
            payload += key
            payload[1] += len(key)
        payload += [calc_crc8(payload[2:])]
    return bytes(payload)

def get_init_seq(key=None):
    return get_telemetry_seq(INIT_SEQ, key)

def get_bind_seq(key=None):
    return get_telemetry_seq(BIND_SEQ, key)

def get_model_seq(model):
    return get_telemetry_seq(MODEL_SEQ, model)
