#!/usr/bin/python

import os
from random import randint
import argparse
import json
from json import JSONEncoder
import mmap
import hashlib
from enum import Enum
import shutil

import firmware
from firmware import DeviceType, FirmwareOptions, RadioType, MCUType, TXType
import UnifiedConfiguration
import binary_flash
from binary_flash import UploadMethod
from external import jmespath

class RegulatoryDomain(Enum):
    us_433 = 'us_433'
    us_433_wide = 'us_433_wide'
    eu_433 = 'eu_433'
    au_433 = 'au_433'
    in_866 = 'in_866'
    eu_868 = 'eu_868'
    au_915 = 'au_915'
    fcc_915 = 'fcc_915'

    def __str__(self):
        return self.value

def generateUID(phrase):
    uid = [
        int(item) if item.isdigit() else -1
        for item in phrase.split(',')
    ]
    if (4 <= len(uid) <= 6) and all(ele >= 0 and ele < 256 for ele in uid):
        # Extend the UID to 6 bytes, as only 4 are needed to bind
        uid = [0] * (6 - len(uid)) + uid
        uid = bytes(uid)
    else:
        uid = hashlib.md5(("-DMY_BINDING_PHRASE=\""+phrase+"\"").encode()).digest()[0:6]
    return uid

def FREQ_HZ_TO_REG_VAL_SX127X(freq):
    return int(freq/61.03515625)

def FREQ_HZ_TO_REG_VAL_SX1280(freq):
    return int(freq/(52000000.0/pow(2,18)))

def domain_number(domain):
    if domain == RegulatoryDomain.au_915:
        return 0
    elif domain == RegulatoryDomain.fcc_915:
        return 1
    elif domain == RegulatoryDomain.eu_868:
        return 2
    elif domain == RegulatoryDomain.in_866:
        return 3
    elif domain == RegulatoryDomain.au_433:
        return 4
    elif domain == RegulatoryDomain.eu_433:
        return 5
    elif domain == RegulatoryDomain.us_433:
        return 6
    elif domain == RegulatoryDomain.us_433_wide:
        return 7

def patch_unified(args, options):
    json_flags = {}
    if args.phrase is not None:
        json_flags['uid'] = [x for x in generateUID(args.phrase)]
    if args.ssid is not None:
        json_flags['wifi-ssid'] = args.ssid
    if args.password is not None and args.ssid is not None:
        json_flags['wifi-password'] = args.password
    if args.auto_wifi is not None:
        json_flags['wifi-on-interval'] = args.auto_wifi

    if args.tlm_report is not None:
        json_flags['tlm-interval'] = args.tlm_report
    if args.fan_min_runtime is not None:
        json_flags['fan-runtime'] = args.fan_min_runtime

    if args.airport_baud is not None:
        json_flags['is-airport'] = True
        if options.deviceType is DeviceType.RX:
            json_flags['rcvr-uart-baud'] = args.airport_baud
        else:
            json_flags['airport-uart-baud'] = args.airport_baud
    elif args.rx_baud is not None:
        json_flags['rcvr-uart-baud'] = args.rx_baud

    if args.lock_on_first_connection is not None:
        json_flags['lock-on-first-connection'] = args.lock_on_first_connection

    if args.domain is not None:
        json_flags['domain'] = domain_number(args.domain)

    json_flags['flash-discriminator'] = randint(1,2**32-1)

    UnifiedConfiguration.doConfiguration(
        args.file,
        JSONEncoder().encode(json_flags),
        args.target,
        'tx' if options.deviceType is DeviceType.TX else 'rx',
        '2400' if options.radioChip is RadioType.SX1280 else '900' if options.radioChip is RadioType.SX127X else 'dual',
        '32' if options.mcuType is MCUType.ESP32 and options.deviceType is DeviceType.RX else '',
        options.luaName,
        args.rx_as_tx
    )

def length_check(l, f):
    def x(s):
        if (len(s) > l):
            raise argparse.ArgumentTypeError(f'too long, {l} chars max')
        else:
            return s
    return x

def ask_for_firmware(args):
    moduletype = 'tx' if args.tx else 'rx'
    with open('hardware/targets.json') as f:
        targets = json.load(f)
        products = []
        if args.target is not None:
            target = args.target
            config = jmespath.search('.'.join(map(lambda s: f'"{s}"', args.target.split('.'))), targets)
        else:
            i = 0
            for k in jmespath.search(f'*.["{moduletype}_2400","{moduletype}_900","{moduletype}_dual"][].*[]', targets):
                i += 1
                products.append(k)
                print(f"{i}) {k['product_name']}")
            print('Choose a configuration to flash')
            choice = input()
            if choice != "":
                config = products[int(choice)-1]
                for v in targets:
                    for t in targets[v]:
                        if t != 'name':
                            for m in targets[v][t]:
                                if targets[v][t][m]['product_name'] == config['product_name']:
                                    target = f'{v}.{t}.{m}'
    return target, config

class readable_dir(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        prospective_dir=values
        if not os.path.isdir(prospective_dir):
            raise argparse.ArgumentTypeError("readable_dir:{0} is not a valid path".format(prospective_dir))
        if os.access(prospective_dir, os.R_OK):
            setattr(namespace,self.dest,prospective_dir)
        else:
            raise argparse.ArgumentTypeError("readable_dir:{0} is not a readable dir".format(prospective_dir))

class writeable_dir(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        prospective_dir=values
        if not os.path.isdir(prospective_dir):
            raise argparse.ArgumentTypeError("readable_dir:{0} is not a valid path".format(prospective_dir))
        if os.access(prospective_dir, os.W_OK):
            setattr(namespace,self.dest,prospective_dir)
        else:
            raise argparse.ArgumentTypeError("readable_dir:{0} is not a writeable dir".format(prospective_dir))

class deprecate_action(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        delattr(namespace, self.dest)

def main():
    parser = argparse.ArgumentParser(description="Configure Binary Firmware")
    # firmware/targets directory
    parser.add_argument('--dir', action=readable_dir, default=None, help='The directory that contains the "hardware" and other firmware directories')
    parser.add_argument('--fdir', action=readable_dir, default=None, help='If specified, then the firmware files are loaded from this directory')
    # Bind phrase
    parser.add_argument('--phrase', type=str, help='Your personal binding phrase')
    parser.add_argument('--flash-discriminator', type=int, default=randint(1,2**32-1), dest='flash_discriminator', help='Force a fixed flash-descriminator instead of random')
    # WiFi Params
    parser.add_argument('--ssid', type=length_check(32, "ssid"), required=False, help='Home network SSID')
    parser.add_argument('--password', type=length_check(64, "password"), required=False, help='Home network password')
    parser.add_argument('--auto-wifi', type=int, help='Interval (in seconds) before WiFi auto starts, if no connection is made')
    parser.add_argument('--no-auto-wifi', action='store_true', help='Disables WiFi auto start if no connection is made')
    # AirPort
    parser.add_argument('--airport-baud', type=int, const=None, nargs='?', action='store', help='If configured as an AirPort device then this is the baud rate to use')
    # RX Params
    parser.add_argument('--rx-baud', type=int, const=420000, nargs='?', action='store', help='The receiver baudrate talking to the flight controller')
    parser.add_argument('--lock-on-first-connection', dest='lock_on_first_connection', action='store_true', help='Lock RF mode on first connection')
    parser.add_argument('--no-lock-on-first-connection', dest='lock_on_first_connection', action='store_false', help='Do not lock RF mode on first connection')
    parser.set_defaults(lock_on_first_connection=None)
    # TX Params
    parser.add_argument('--tlm-report', type=int, const=240, nargs='?', action='store', help='The interval (in milliseconds) between telemetry packets')
    parser.add_argument('--fan-min-runtime', type=int, const=30, nargs='?', action='store', help='The minimum amount of time the fan should run for (in seconds) if it turns on')
    # Regulatory domain
    parser.add_argument('--domain', type=RegulatoryDomain, choices=list(RegulatoryDomain), default=None, help='For SX127X based devices, which regulatory domain is being used')
    # Unified target
    parser.add_argument('--target', type=str, help='Unified target JSON path')
    # Flashing options
    parser.add_argument("--flash", type=UploadMethod, choices=list(UploadMethod), help="Flashing Method")
    parser.add_argument("--erase", action='store_true', default=False, help="Full chip erase before flashing on ESP devices")
    parser.add_argument('--out', action=writeable_dir, default=None)
    parser.add_argument("--port", type=str, help="SerialPort or WiFi address to flash firmware to")
    parser.add_argument("--baud", type=int, default=0, help="Baud rate for serial communication")
    parser.add_argument("--force", action='store_true', default=False, help="Force upload even if target does not match")
    parser.add_argument("--confirm", action='store_true', default=False, help="Confirm upload if a mismatched target was previously uploaded")
    parser.add_argument("--tx", action='store_true', default=False, help="Flash a TX module, RX if not specified")
    parser.add_argument("--lbt", action='store_true', default=False, help="Use LBT firmware, default is FCC (only for 2.4GHz firmware)")
    parser.add_argument('--rx-as-tx', type=TXType, choices=list(TXType), required=False, default=None, help="Flash an RX module with TX firmware, either internal (full-duplex) or external (half-duplex)")
    # Deprecated options, left for backward compatibility
    parser.add_argument('--uart-inverted', action=deprecate_action, nargs=0, help='Deprecated')
    parser.add_argument('--no-uart-inverted', action=deprecate_action, nargs=0, help='Deprecated')

    #
    # Firmware file to patch/configure
    parser.add_argument("file", nargs="?", type=argparse.FileType("r+b"))

    args = parser.parse_args()

    if args.dir is not None:
        os.chdir(args.dir)

    if args.file is None:
        args.target, config = ask_for_firmware(args)
        try:
            file = config['firmware']
            if args.rx_as_tx is not None:
                if config['platform'].startswith('esp32') or config['platform'].startswith('esp8285') and args.rx_as_tx == TXType.internal:
                    file = file.replace('_RX', '_TX')
                else:
                    print("Selected device cannot operate as 'RX-as-TX' of this type.")
                    print("ESP8285 only supports full-duplex internal RX as TX.")
                    exit(1)
            firmware_dir = '' if args.fdir is None else args.fdir + '/'
            srcdir = firmware_dir + ('LBT/' if args.lbt else 'FCC/') + file
            dst = 'firmware.bin'
            shutil.copy2(srcdir + '/firmware.bin', ".")
            if os.path.exists(srcdir + '/bootloader.bin'): shutil.copy2(srcdir + '/bootloader.bin', ".")
            if os.path.exists(srcdir + '/partitions.bin'): shutil.copy2(srcdir + '/partitions.bin', ".")
            if os.path.exists(srcdir + '/boot_app0.bin'): shutil.copy2(srcdir + '/boot_app0.bin', ".")
            args.file = open(dst, 'r+b')
        except FileNotFoundError:
            print("Firmware files not found, did you download and unpack them in this directory?")
            exit(1)
    else:
        args.target, config = ask_for_firmware(args)

    with args.file as f:
        mm = mmap.mmap(f.fileno(), 0)

        pos = firmware.get_hardware(mm)
        options = FirmwareOptions(
            MCUType.ESP32 if config['platform'].startswith('esp32') else MCUType.ESP8266,
            DeviceType.RX if '.rx_' in args.target else DeviceType.TX,
            RadioType.SX127X if '_900.' in args.target else RadioType.SX1280 if '_2400.' in args.target else RadioType.LR1121,
            config['lua_name'] if 'lua_name' in config else '',
            config['stlink']['bootloader'] if 'stlink' in config else '',
            config['stlink']['offset'] if 'stlink' in config else 0,
            config['firmware']
        )
        patch_unified(args, options)
        args.file.close()

        if options.mcuType == MCUType.ESP8266:
            import gzip
            with open(args.file.name, 'rb') as f_in:
                with gzip.open('firmware.bin.gz', 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)

        if args.flash:
            args.target = config.get('firmware')
            args.accept = config.get('prior_target_name')
            args.platform = config.get('platform')
            return binary_flash.upload(options, args)
        elif 'upload_methods' in config and 'stock' in config['upload_methods']:
            shutil.copy(args.file.name, 'firmware.elrs')
    return 0

if __name__ == '__main__':
    try:
        exit(main())
    except AssertionError as e:
        print(e)
        exit(1)
