import json
import re

def appendConfiguration(source, target, env):
    target_name = env.get('PIOENV', '').upper()
    parts = re.search("(.*)_VIA_.*", target_name)
    if parts and parts.group(1):
        target_name = parts.group(1).replace('_', ' ')
    with open(str(target[0]), 'a+b') as f:
        dev = (env.get('DEVICE_NAME', target_name).encode() + (b'\0' * 16))[0:16]
        f.write(dev)
        options = (json.JSONEncoder().encode(env['OPTIONS_JSON']).encode() + (b'\0' * 512))[0:512]
        f.write(options)
        try:
            with open('data/hardware.json') as h:
                for line in h:
                    f.write(line.encode())
        except EnvironmentError:
            None
        f.write(b'\0')
