import os, sys
import json
import glob
import argparse
import hardware

hadError = False
warnEnabled = False
firmwares = set()

def error(msg):
    global hadError
    hadError = True
    print(msg)

def warn(msg):
    if warnEnabled:
        global hadError
        hadError = True
        print(msg)

def validate_stm32(vendor, type, devname, device):
    for method in device['upload_methods']:
        if method not in ['stlink', 'dfu', 'uart', 'wifi', 'betaflight', 'stock']:
            error(f'Invalid upload method "{method}" for target "{vendor}.{type}.{devname}"')
    if 'stlink' not in device['upload_methods']:
        error(f'STM32 based devices must always have "stlink" as an upload_method for target "{vendor}.{type}.{devname}"')
    if 'stlink' not in device:
        error(f'STM32 based devices must always have "stlink" attribute for target "{vendor}.{type}.{devname}"')
    else:
        stlink = device['stlink']
        if 'cpus' not in stlink:
            error(f'The "stlink" attribute for target "{vendor}.{type}.{devname}" must have a list of valid "cpus"')
        if 'offset' not in stlink:
            error(f'The "stlink" attribute for target "{vendor}.{type}.{devname}" must have a valid "offset"')
        if 'bootloader' not in stlink:
            error(f'The "stlink" attribute for target "{vendor}.{type}.{devname}" must have a valid "bootloader"')
    # could check the existence of the bootloader file

def validate_esp(vendor, type, devname, device):
    if 'lua_name' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "lua_name" child element')
    if len(device['lua_name']) > 16:
        error(f'device "{vendor}.{type}.{devname}" must have a "lua_name" of 16 characters or less')
    # validate layout_file
    if not device['firmware'].startswith('Unified'):
        error(f'ESP target "{vendor}.{type}.{devname}" must be using a Unified firmware')
    if 'layout_file' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "layout_file" child element')
    else:
        dir = ('RX/' if type.startswith('rx') else 'TX/')
        layout_file = device['layout_file']
        if not os.path.isfile(dir + layout_file):
            error(f'File specified by layout_file "{layout_file}" in target "{vendor}.{type}.{devname}", does not exist')
        else:
            # load file, merge overlay, call validate_hardware
            with open(dir + layout_file, 'r') as f:
                global hadError
                layout = json.load(f)
                if 'overlay' in device:
                    layout.update(device['overlay'])
                hadError |= hardware.validate(f'{vendor}.{type}.{devname}', layout, device, type)

    # could validate overlay
    if 'prior_target_name' not in device:
        warn(f'device "{vendor}.{type}.{devname}" should have a "prior_target_name" child element')


def validate_esp32(vendor, type, devname, device):
    for method in device['upload_methods']:
        if method not in ['uart', 'etx', 'wifi', 'betaflight']:
            error(f'Invalid upload method "{method}" for target "{vendor}.{type}.{devname}"')
    validate_esp(vendor, type, devname, device)

def validate_esp8285(vendor, type, devname, device):
    for method in device['upload_methods']:
        if method not in ['uart', 'wifi', 'betaflight']:
            error(f'Invalid upload method "{method}" for target "{vendor}.{type}.{devname}"')
    validate_esp(vendor, type, devname, device)

def validate_devices(vendor, type, devname, device):
    if devname != devname.lower():
        error(f'device tag "{devname}" should be lowercase')
    if 'product_name' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "product_name" child element')
    if 'upload_methods' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "upload_methods" child element')
    if 'min_version' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "min_version" child element')

    if 'firmware' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "firmware" child element')
    else:
        firmware = device['firmware']
        if len(firmwares) != 0 and firmware not in firmwares:
            error(f'device "{vendor}.{type}.{devname}" has an invalid firmware file "{firmware}"')
        elif (firmware.endswith('_TX') and 'tx_' not in type):
            error(f'device "{vendor}.{type}.{devname}" has an invalid firmware file "{firmware}", it must be a TX target firmware')
        elif (firmware.endswith('_RX') and 'rx_' not in type):
            error(f'device "{vendor}.{type}.{devname}" has an invalid firmware file "{firmware}", it must be an RX target firmware')

    if 'platform' not in device:
        error(f'device "{vendor}.{type}.{devname}" must have a "platform" child element')
    else:
        platform = device['platform']
        if platform == 'stm32':
            validate_stm32(vendor, type, devname, device)
        elif platform == 'esp8285':
            validate_esp8285(vendor, type, devname, device)
        elif platform.startswith('esp32'):
            validate_esp32(vendor, type, devname, device)
        else:
            error(f'invalid platform "{platform}" in device "{vendor}.{type}.{devname}"')

    if 'features' in device:
        for feature in device['features']:
            if feature not in ['buzzer', 'unlock-higher-power', 'fan', 'sbus-uart']:
                error(f'features must contain one or more of [\'buzzer\', \'unlock-higher-power\', \'fan\', \'sbus-uart\'], if present in target "{vendor}.{type}.{devname}"')

def validate_vendor(name, types):
    if name != name.lower():
        error(f'vendor tag "{vendor}" should be lowercase')

    if 'name' not in types:
        error(f'vendor "{vendor}" must have a "name" child element')

    for type in types:
        if type not in ['rx_2400', 'rx_900', 'rx_dual', 'tx_2400', 'tx_900', 'tx_dual', 'name']:
            error(f'invalid tag "{type}" in "{vendor}"')
        if type in  ['rx_2400', 'rx_900', 'rx_dual', 'tx_2400', 'tx_900', 'tx_dual']:
            for device in types[type]:
                validate_devices(name, type, device, types[type][device])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Configure Binary Firmware")
    parser.add_argument("--warn", "-w", action='store_true', default=False, help="Print warnings")
    args = parser.parse_args()
    warnEnabled = args.warn

    targets = {}
    with open('targets.json') as f:
        targets = json.load(f)

        for inifile in glob.iglob('../targets/*.ini'):
            with open(inifile) as ini:
                for line in ini:
                    if line.startswith('[env:'):
                        try:
                            firmware_file = line[5:line.index('_via_')]
                            firmwares.add(firmware_file)
                        except ValueError:
                            print(line)
                            None

        for vendor in targets:
            validate_vendor(vendor, targets[vendor])

        for inifile in glob.iglob('targets/*.ini'):
            with open(inifile) as ini:
                for line in ini:
                    if line.startswith('[env:'):
                        target = line
                    if line.startswith('board_config'):
                        eq = line.find('=')
                        board = line[eq+1:].strip()
                        parts = board.split('.')
                        if len(parts) != 3:
                            error(f'{inifile}: board_config must have 3 parts')
                        else:
                            vendor = parts[0]
                            type = parts[1]
                            device = parts[2]
                            if vendor not in targets:
                                error(f'{inifile}: targets.json file does not contain vendor {vendor}')
                            elif type not in targets[vendor]:
                                error(f'{inifile}: targets.json "{vendor}" does not contain type {type}')
                            elif device not in targets[vendor][type]:
                                error(f'{inifile}: targets.json "{vendor}.{type}" does not contain device {device}')
    if hadError:
        sys.exit(1)
    sys.exit(0)