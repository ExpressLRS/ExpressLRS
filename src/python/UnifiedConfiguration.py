import json
import re
import sys

from external import jmespath

def appendToFirmware(firmware_file, product_name, lua_name, options, layout_file):
    product = (product_name.encode() + (b'\0' * 128))[0:128]
    device = (lua_name.encode() + (b'\0' * 16))[0:16]
    with open(firmware_file, 'a+b') as f:
        f.write(product)
        f.write(device)
        options = (options.encode() + (b'\0' * 512))[0:512]
        f.write(options)
        if layout_file is not None:
            try:
                with open(layout_file) as h:
                    hardware = json.load(h)
                    f.write(json.JSONEncoder().encode(hardware).encode())
            except EnvironmentError:
                None
        f.write(b'\0')


def appendConfiguration(source, target, env):
    target_name = env.get('PIOENV', '').upper()
    moduletype = 'tx' if '_TX_' in target_name else 'rx'
    frequency = '2400' if '_2400_' in target_name else '900'
    config = env.GetProjectOption('board_config', None)

    if 'UNIFIED_' not in target_name and config is None:
        return

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
        print('Not running in an interactive shell, so leaving the firmware "bare".')
        print('You will be able to configure the hardware via the web UI on the device.')
    else:
        products = []
        i = 0
        for k in jmespath.search(f'*."{moduletype}_{frequency}".*[].product_name', targets):
            i += 1
            products.append(k)
            print(f"{i}) {k}")
        print('Choose a configuration to load into the firmware file (press enter to leave bare)')
        choice = input()
        if choice != "":
            config = products[int(choice)-1]
            config = jmespath.search(f'[[*."{moduletype}_{frequency}"][].*][][?product_name==`{config}`][]', targets)[0]

    if config is not None:
        product_name = config['product_name']
        lua_name = config['lua_name']
        dir = 'TX' if moduletype == 'tx' else 'RX'
        layout = f"hardware/{dir}/{config['layout_file']}"

    options = json.JSONEncoder().encode(env['OPTIONS_JSON'])
    appendToFirmware(str(target[0]), product_name, lua_name, options, layout)
