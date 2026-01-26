#!/usr/bin/python

import argparse
import re
import json
import struct
import sys

from external import jmespath
from firmware import TXType


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

def appendToFirmware(firmware_file, product_name, lua_name, defines, config, layout_file, rx_as_tx):
    product = (product_name.encode() + (b'\0' * 128))[0:128]
    device = (lua_name.encode() + (b'\0' * 16))[0:16]
    end = findFirmwareEnd(firmware_file)
    firmware_file.seek(end, 0)
    firmware_file.write(product)
    firmware_file.write(device)
    defines = (defines.encode() + (b'\0' * 512))[0:512]
    firmware_file.write(defines)
    if layout_file is not None:
        try:
            with open(layout_file) as h:
                hardware = json.load(h)
                if 'overlay' in config:
                    hardware.update(config['overlay'])
                if rx_as_tx is not None:
                    if 'serial_rx' not in hardware or 'serial_tx' not in hardware:
                        sys.stderr.write(f'Cannot select this target as RX-as-TX\n')
                        exit(1)
                    if rx_as_tx == TXType.external and hardware['serial_rx']:
                        hardware['serial_rx'] = hardware['serial_tx']
                    if 'led_red' not in hardware and 'led' in hardware:
                        hardware['led_red'] = hardware['led']
                        del hardware['led']
                layout = (json.JSONEncoder().encode(hardware).encode() + (b'\0' * 2048))[0:2048]
                firmware_file.write(layout)
        except EnvironmentError:
            sys.stderr.write(f'Error opening file "{layout_file}"\n')
            exit(1)
    else:
        firmware_file.write(b'\0' * 2048)
    if config is not None and 'logo_file' in config:
        logo_file = f"hardware/logo/{config['logo_file']}"
        with open(logo_file, 'rb') as f:
            firmware_file.write(f.read())
    if config is not None and 'prior_target_name' in config:
        firmware_file.write(b'\xBE\xEF\xCA\xFE')
        firmware_file.write(config['prior_target_name'].upper().encode())
        firmware_file.write(b'\0')

# Return the product name for the last hardware.json that was appended for the given PIO env target
# e.g. Unified_ESP32_LR1121_via_WIFI -> "RadioMaster Nomad 2.4/900 TX"
def getDefaultProductForTarget(target_name: str) -> str:
    if target_name is None or target_name == '':
        return ''

    # remove the "_via_WIFI" etc method
    target_wo_method = re.sub('_VIA_.*', '', target_name.upper())
    try:
        with open('.pio/default_target_config.json', 'r') as f:
            data = json.load(f)
            product_name = data.get(target_wo_method)
            return product_name
    except:
        # No file or json.JSONDecodeError
        return ''

# Save the product name of a hardware configuration appended to a target, for a future default
# target_name: e.g. Unified_ESP32_LR1121_via_WIFI
# product_name: e.g. RadioMaster Nomad 2.4/900 TX
def setDefaultProductForTarget(target_name: str, product_name: str) -> None:
    if target_name is None or target_name == '':
        return

    # remove the "_via_WIFI" etc method
    target_wo_method = re.sub('_VIA_.*', '', target_name.upper())
    data = {}
    try:
        with open('.pio/default_target_config.json', 'r') as f:
            data = json.load(f)
    except:
        # No file or json.JSONDecodeError
        pass

    data[target_wo_method] = product_name
    with open('.pio/default_target_config.json', 'w') as f:
        json.dump(data, f, indent=4)

# Remove the default product name for a given target (e.g. Unified_ESP32_LR1121_via_WIFI)
# so there will not be a default on future runs. Used in "clean" operation
def clearDefaultProductForTarget(target_name: str) -> None:
    # remove the "_via_WIFI" etc method
    target_wo_method = re.sub('_VIA_.*', '', target_name.upper())
    data = {}
    try:
        with open('.pio/default_target_config.json', 'r') as f:
            data = json.load(f)
    except:
        # No file or json.JSONDecodeError
        pass

    data.pop(target_wo_method, None)
    with open('.pio/default_target_config.json', 'w') as f:
        json.dump(data, f, indent=4)

def is_pio_upload():
    # SCons.Script is only available when run via 'pio run'?
    scons_module = sys.modules.get("SCons.Script")
    if not scons_module:
        return False

    try:
        # COMMAND_LINE_TARGETS = ['upload'] when this is an upload build
        targets = getattr(scons_module, "COMMAND_LINE_TARGETS", [])
        return "upload" in targets
    except Exception:
        return False

def interactiveProductSelect(targets: dict, target_name: str, moduletype: str, frequency: str, platform: str) -> dict:
    products = []
    for k in jmespath.search(f'[*."{moduletype}_{frequency}".*][][?platform==`{platform}`][]', targets):
        products.append(k)
    if frequency == 'dual':
        for k in jmespath.search(f'[*."{moduletype}_2400".*][][?platform==`{platform}`][]', targets):
            if '_LR1121_' in k['firmware']:
                products.append(k)
        for k in jmespath.search(f'[*."{moduletype}_900".*][][?platform==`{platform}`][]', targets):
            if '_LR1121_' in k['firmware']:
                products.append(k)

    if not products:
        return None

    # Sort the list by product name, case insensitive, and print the list
    products = sorted(products, key=lambda p: p['product_name'].casefold())
    # Find a default if this target has been build before
    default_prod = getDefaultProductForTarget(target_name)
    # Make sure default_conf is a valid product name, set to '0' if not or default_prod is blank
    default_prod = default_prod if any(p['product_name'] == default_prod for p in products) else '0'

    if (default_prod != '0') and is_pio_upload():
        print(f'Upload using default product "{default_prod}" (use Clean to clear default)')
        choice = default_prod
    else:
        print(f'0) Leave bare (no configuration)')
        for i, p in enumerate(products):
            print(f"{i+1}) {p['product_name']}")
        print(f'default) {default_prod}')
        print('Choose a configuration to load into the firmware file')

        choice = input()
        if choice == '':
            choice = default_prod
        if choice == '0':
            return None

    # First see if choice is a valid product name from the list
    config = next((p for p in products if p['product_name'] == choice), None)
    # else choice is an integer
    config = config if config else products[int(choice)-1]

    return config

def doConfiguration(file, defines, config, target_name, moduletype, frequency, platform, device_name, rx_as_tx):
    product_name = "Unified"
    lua_name = "Unified"
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
        config = interactiveProductSelect(targets, target_name, moduletype, frequency, platform)

    if config is not None:
        product_name = config['product_name']
        setDefaultProductForTarget(target_name, product_name)
        lua_name = config['lua_name']
        dir = 'TX' if moduletype == 'tx' else 'RX'
        layout = f"hardware/{dir}/{config['layout_file']}"

    lua_name = lua_name if device_name is None else device_name
    appendToFirmware(file, product_name, lua_name, defines, config, layout, rx_as_tx)

def appendConfiguration(source, target, env):
    target_name = env.get('PIOENV', '').upper()
    device_name = env.get('DEVICE_NAME', None)
    config = env.GetProjectOption('board_config', None)
    if 'UNIFIED_' not in target_name and config is None:
        return

    moduletype = ''
    frequency = ''
    if config is not None:
        moduletype = 'tx' if '.tx_' in config else 'rx'
        frequency = '2400' if '_2400.' in config else '900' if '_900.' in config else 'dual'
    else:
        moduletype = 'tx' if '_TX_' in target_name else 'rx'
        frequency = '2400' if '_2400_' in target_name else '900' if '_900_' in target_name else 'dual'

    if env.get('PIOPLATFORM', '') == 'espressif32':
        platform = 'esp32'
        if 'esp32-s3' in env.get('BOARD', ''):
            platform = 'esp32-s3'
        elif 'esp32-c3' in env.get('BOARD', ''):
            platform = 'esp32-c3'
    else:
        platform = 'esp8285'

    defines = json.JSONEncoder().encode(env['OPTIONS_JSON'])

    with open(str(target[0]), "r+b") as firmware_file:
        doConfiguration(firmware_file, defines, config, target_name, moduletype, frequency, platform, device_name, None)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Configure Unified Firmware")
    parser.add_argument("--target", type=str, help="Path into 'targets.json' file for hardware configuration")
    parser.add_argument("--options", type=str, help="JSON document with configuration options")

    parser.add_argument("file", type=argparse.FileType("r+b"), help="The firmware file to configure")

    args = parser.parse_args()

    targets = {}
    with open('hardware/targets.json') as f:
        targets = json.load(f)

    moduletype = 'tx' if '.tx_' in args.target else 'rx'

    config ='.'.join(map(lambda s: f'"{s}"', args.target.split('.')))
    config = jmespath.search(config, targets)

    if config is not None:
        product_name = config['product_name']
        lua_name = config['lua_name']
        dir = 'TX' if moduletype == 'tx' else 'RX'
        layout = f"hardware/{dir}/{config['layout_file']}"

    appendToFirmware(args.file, product_name, lua_name, args.options, config, layout, None)
