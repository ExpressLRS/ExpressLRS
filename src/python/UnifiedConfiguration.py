import argparse
import json
import re
import struct
import sys

from external import jmespath

def findFirmwareEnd(f):
    f.seek(0, 0)
    (magic, segments, _, _, _) = struct.unpack('<BBBBI', f.read(8))
    if magic != 0xe9:
        sys.stderr.write('The file provided does not the right magic for a firmware file!\n')
        exit(1)

    is8285 = False
    if segments == 2: # we have to assume it's an ESP8266/85
        f.seek(0x1000, 0)
        (magic, segments, _, _, _) = struct.unpack('<BBBBI', f.read(8))
        is8285 = True
    else:
        f.seek(24, 0)

    for _ in range(segments):
        (_, size) = struct.unpack('<II', f.read(8))
        f.seek(size, 1)

    pos = f.tell()
    pos = (pos + 16) & ~15
    if not is8285:
        pos = pos + 32
    return pos

def appendToFirmware(firmware_file, product_name, lua_name, options, layout_file):
    product = (product_name.encode() + (b'\0' * 128))[0:128]
    device = (lua_name.encode() + (b'\0' * 16))[0:16]
    end = findFirmwareEnd(firmware_file)
    firmware_file.seek(end, 0)
    firmware_file.write(product)
    firmware_file.write(device)
    options = (options.encode() + (b'\0' * 512))[0:512]
    firmware_file.write(options)
    if layout_file is not None:
        try:
            with open(layout_file) as h:
                hardware = json.load(h)
                firmware_file.write(json.JSONEncoder().encode(hardware).encode())
        except EnvironmentError:
            sys.stderr.write(f'Error opening file "{layout_file}"\n')
            exit(1)
    firmware_file.write(b'\0')


def configureFirmware(file, config, options):
    targets = {}
    with open('hardware/targets.json') as f:
        targets = json.load(f)

    moduletype = 'tx' if '.tx_' in config else 'rx'

    config ='.'.join(map(lambda s: f'"{s}"', config.split('.')))
    config = jmespath.search(config, targets)

    if config is not None:
        product_name = config['product_name']
        lua_name = config['lua_name']
        dir = 'TX' if moduletype == 'tx' else 'RX'
        layout = f"hardware/{dir}/{config['layout_file']}"

    appendToFirmware(file, product_name, lua_name, options, layout)


def appendConfiguration(source, target, env):
    target_name = env.get('PIOENV', '').upper()
    config = env.GetProjectOption('board_config', None)
    if 'UNIFIED_' not in target_name and config is None:
        return

    moduletype = ''
    frequency = ''
    if config is not None:
        moduletype = 'tx' if '.tx_' in config else 'rx'
        frequency = '2400' if '_2400.' in config else '900'
    else:
        moduletype = 'tx' if '_TX_' in target_name else 'rx'
        frequency = '2400' if '_2400_' in target_name else '900'

    platform = '32' if moduletype == 'rx' and env.get('PIOPLATFORM', '') in ['espressif32'] else ''

    parts = re.search('(.*)_VIA_.*', target_name)
    if parts and parts.group(1):
        target_name = parts.group(1).replace('_', ' ')

    product_name = target_name
    lua_name = target_name
    layout = None

    targets = {}
    with open('hardware/targets.json') as f:
        targets = json.load(f)

    if config is not None:
        config ='.'.join(map(lambda s: f'"{s}"', config.split('.')))
        config = jmespath.search(config, targets)
    elif not sys.stdin.isatty():
        print('Not running in an interactive shell, leaving the firmware "bare".\n')
        print('The current compile options (user defines) have been included.')
        print('You will be able to configure the hardware via the web UI on the device.')
    else:
        products = []
        i = 0
        for k in jmespath.search(f'*."{moduletype}{platform}_{frequency}".*[].product_name', targets):
            i += 1
            products.append(k)
            print(f"{i}) {k}")
        print('Choose a configuration to load into the firmware file (press enter to leave bare)')
        choice = input()
        if choice != "":
            config = products[int(choice)-1]
            config = jmespath.search(f'[[*."{moduletype}{platform}_{frequency}"][].*][][?product_name==`{config}`][]', targets)[0]

    if config is not None:
        product_name = config['product_name']
        lua_name = config['lua_name']
        dir = 'TX' if moduletype == 'tx' else 'RX'
        layout = f"hardware/{dir}/{config['layout_file']}"

    options = json.JSONEncoder().encode(env['OPTIONS_JSON'])
    with open(str(target[0]), "r+b") as firmware_file:
        appendToFirmware(firmware_file, product_name, lua_name, options, layout)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Configure Unified Firmware")
    parser.add_argument("--target", type=str, help="Path into 'targets.json' file for hardware configuration")
    parser.add_argument("--options", type=str, help="JSON document with configuration options")

    parser.add_argument("file", type=argparse.FileType("r+b"), help="The firmware file to configure")

    args = parser.parse_args()

    configureFirmware(args.file, args.target, args.options)
