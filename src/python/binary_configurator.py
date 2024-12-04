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
import melodyparser
import UnifiedConfiguration
import binary_flash
from binary_flash import UploadMethod
from external import jmespath

class BuzzerMode(Enum):
    quiet = 'quiet'
    one = 'one-beep'
    beep = 'beep-tune'
    default = 'default-tune'
    custom = 'custom-tune'

    def __str__(self):
        return self.value

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

def write32(mm, pos, val):
    if val != None:
        mm[pos + 0] = (val >> 0) & 0xFF
        mm[pos + 1] = (val >> 8) & 0xFF
        mm[pos + 2] = (val >> 16) & 0xFF
        mm[pos + 3] = (val >> 24) & 0xFF
    return pos + 4

def read32(mm, pos):
    val = mm[pos + 0]
    val += mm[pos + 1] << 8
    val += mm[pos + 2] << 16
    val += mm[pos + 3] << 24
    return pos + 4, val

def writeString(mm, pos, string, maxlen):
    if string != None:
        l = len(string)
        if l > maxlen-1:
            l = maxlen-1
        mm[pos:pos+l] = string.encode()[0,l]
        mm[pos+l] = 0
    return pos + maxlen

def readString(mm, pos, maxlen):
    val = mm[pos:mm.find(b'\x00', pos)].decode()
    return pos + maxlen, val

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

def patch_uid(mm, pos, args):
    if (args.phrase):
        mm[pos] = 1
        mm[pos+1:pos + 7] = generateUID(args.phrase)
    pos += 7
    return pos

def patch_flash_discriminator(mm, pos, args):
    return write32(mm, pos, args.flash_discriminator)

def patch_wifi(mm, pos, args):
    interval = None
    if args.no_auto_wifi:
        interval = -1
    elif args.auto_wifi:
        interval = args.auto_wifi * 1000
    pos = write32(mm, pos, interval)
    pos = writeString(mm, pos, args.ssid, 33)
    pos = writeString(mm, pos, args.password, 65)
    return pos

def patch_rx_params(mm, pos, args):
    pos = write32(mm, pos, args.rx_baud if args.airport_baud is None else args.airport_baud)
    val = mm[pos]
    val &= ~1   # unused1 - ex invert_tx
    if args.lock_on_first_connection != None:
        val &= ~2
        val |= (args.lock_on_first_connection << 1)
    val &= ~4   # unused2 - ex r9mm_mini_sbus
    if args.airport_baud != None:
        val |= ~8
        val |= 0 if args.airport_baud == 0 else 8
    mm[pos] = val
    return pos + 1

def patch_tx_params(mm, pos, args, options):
    pos = write32(mm, pos, args.tlm_report)
    val = mm[pos]
    val &= ~1   # unused1 - ex uart_inverted
    if args.unlock_higher_power != None:
        val &= ~2
        val |= (args.unlock_higher_power << 1)
    if args.airport_baud != None:
        val |= ~4
        val |= 0 if args.airport_baud == 0 else 4
    mm[pos] = val
    pos += 1
    if options.hasBuzzer:
        pos = patch_buzzer(mm, pos, args)
    pos = write32(mm, pos, 0 if args.airport_baud is None else args.airport_baud)
    return pos

def patch_buzzer(mm, pos, args):
    melody = args.buzzer_melody
    if args.buzzer_mode:
        if args.buzzer_mode == BuzzerMode.quiet:
            mm[pos] = 0
        if args.buzzer_mode == BuzzerMode.one:
            mm[pos] = 1
        if args.buzzer_mode == BuzzerMode.beep:
            mm[pos] = 2
            melody = 'A4 20 B4 20|60|0'
        if args.buzzer_mode == BuzzerMode.default:
            mm[pos] = 2
            melody = 'E5 40 E5 40 C5 120 E5 40 G5 22 G4 21|20|0'
        if args.buzzer_mode == BuzzerMode.custom:
            mm[pos] = 2
            melody = args.buzzer_melody
    mode = mm[pos]
    pos += 1

    mpos = 0
    if mode == 2 and melody:
        melodyArray = melodyparser.parseToArray(melody)
        for element in melodyArray:
            if mpos == 32*4:
                print("Melody truncated at 32 tones")
                break
            mm[pos+mpos+0] = (int(element[0]) >> 0) & 0xFF
            mm[pos+mpos+1] = (int(element[0]) >> 8) & 0xFF
            mm[pos+mpos+2] = (int(element[1]) >> 0) & 0xFF
            mm[pos+mpos+3] = (int(element[1]) >> 8) & 0xFF
            mpos += 4
        # If we are short, then add a terminating 0's
        while(mpos < 32*4):
            mm[pos+mpos] = 0
            mpos += 1

    pos += 32*4     # 32 notes x (2 bytes tone, 2 bytes duration)
    return pos

def FREQ_HZ_TO_REG_VAL_SX127X(freq):
    return int(freq/61.03515625)

def FREQ_HZ_TO_REG_VAL_SX1280(freq):
    return int(freq/(52000000.0/pow(2,18)))

def generate_domain(mm, pos, count, init, step):
    pos = write32(mm, pos, count)
    val = init
    for x in range(count):
        pos = write32(mm, pos, FREQ_HZ_TO_REG_VAL_SX127X(val))
        val += step

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

def patch_firmware(options, mm, pos, args):
    if options.mcuType is MCUType.STM32:
        if options.radioChip is RadioType.SX127X and args.domain:
            mm[pos] = domain_number(args.domain)
        pos += 1
        pos = patch_uid(mm, pos, args)
        pos = patch_flash_discriminator(mm, pos, args)
        pos = write32(mm, pos, args.fan_min_runtime)
        if options.deviceType is DeviceType.TX:
            pos = patch_tx_params(mm, pos, args, options)
        elif options.deviceType is DeviceType.RX:
            pos = patch_rx_params(mm, pos, args)
    else:
        patch_unified(args, options)

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
    if args.unlock_higher_power is not None:
        json_flags['unlock-higher-power'] = args.unlock_higher_power
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
    parser.add_argument('--unlock-higher-power', dest='unlock_higher_power', action='store_true', help='DANGER: Unlocks the higher power on modules that do not normally have sufficient cooling e.g. 1W on R9M')
    parser.add_argument('--no-unlock-higher-power', dest='unlock_higher_power', action='store_false', help='Set the max power level at the safe maximum level')
    parser.set_defaults(unlock_higher_power=None)
    # Buzzer
    parser.add_argument('--buzzer-mode', type=BuzzerMode, choices=list(BuzzerMode), default=None, help='Which buzzer mode to use, if there is a buzzer')
    parser.add_argument('--buzzer-melody', type=str, default=None, help='If the mode is "custom", then this is the tune')
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
                    print("STM32 does not support RX as TX.")
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
            False if config['platform'] == 'stm32' else True,
            True if 'features' in config and 'buzzer' in config['features'] else False,
            MCUType.STM32 if config['platform'] == 'stm32' else MCUType.ESP32 if config['platform'].startswith('esp32') else MCUType.ESP8266,
            DeviceType.RX if '.rx_' in args.target else DeviceType.TX,
            RadioType.SX127X if '_900.' in args.target else RadioType.SX1280 if '_2400.' in args.target else RadioType.LR1121,
            config['lua_name'] if 'lua_name' in config else '',
            config['stlink']['bootloader'] if 'stlink' in config else '',
            config['stlink']['offset'] if 'stlink' in config else 0,
            config['firmware']
        )
        patch_firmware(options, mm, pos, args)
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
