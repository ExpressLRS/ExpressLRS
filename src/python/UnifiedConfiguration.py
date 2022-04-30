import json
import re

def appendConfiguration(source, target, env):
    config = env.GetProjectOption('hardware_config', 'data/hardware.json')
    target_name = env.get('PIOENV', '').upper()
    if 'Unified_' in target_name or config is not None:
        parts = re.search('(.*)_VIA_.*', target_name)
        if parts and parts.group(1):
            target_name = parts.group(1).replace('_', ' ')
        device = env.GetProjectOption('device_name', target_name)
        with open(str(target[0]), 'a+b') as f:
            dev = (env.get('DEVICE_NAME', device).encode() + (b'\0' * 16))[0:16]
            f.write(dev)
            options = (json.JSONEncoder().encode(env['OPTIONS_JSON']).encode() + (b'\0' * 512))[0:512]
            f.write(options)
            try:
                with open(config) as h:
                    for line in h:
                        f.write(line.encode())
            except EnvironmentError:
                None
            f.write(b'\0')
