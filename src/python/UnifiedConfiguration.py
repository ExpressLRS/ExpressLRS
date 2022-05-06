import json
import re

def appendConfiguration(source, target, env):
    config = env.GetProjectOption('board_hardware', 'data/hardware.json')
    target_name = env.get('PIOENV', '').upper()
    if 'Unified_' in target_name or config is not None:
        parts = re.search('(.*)_VIA_.*', target_name)
        if parts and parts.group(1):
            target_name = parts.group(1).replace('_', ' ')
        product = (env.GetProjectOption('board_product_name', target_name).encode() + (b'\0' * 128))[0:128]
        device = (env.GetProjectOption('board_lua_name', target_name).encode() + (b'\0' * 16))[0:16]
        with open(str(target[0]), 'a+b') as f:
            f.write(product)
            f.write(device)
            options = (json.JSONEncoder().encode(env['OPTIONS_JSON']).encode() + (b'\0' * 512))[0:512]
            f.write(options)
            try:
                with open(config) as h:
                    hardware = json.load(h)
                    f.write(json.JSONEncoder().encode(hardware).encode())
            except EnvironmentError:
                None
            f.write(b'\0')
