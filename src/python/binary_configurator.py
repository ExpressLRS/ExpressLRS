#!/usr/bin/python

import argparse
from json import JSONEncoder
import mmap
import hashlib
from enum import Enum

import firmware
from firmware import DeviceType, FirmwareOptions, RadioType, MCUType
import melodyparser
import UnifiedConfiguration
import binary_flash
from binary_flash import UploadMethod

class BuzzerMode(Enum):
    quiet = 'quiet'
    one = 'one-beep'
    beep = 'beep-tune'
    default = 'default-tune'
    custom = 'custom-tune'

    def __str__(self):
        return self.value

class RegulatoryDomain(Enum):
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

def patch_uid(mm, pos, args):
    if (args.phrase):
        mm[pos] = 1
        bindingPhraseHash = hashlib.md5(("-DMY_BINDING_PHRASE=\""+args.phrase+"\"").encode()).digest()
        mm[pos+1:pos + 7] = bindingPhraseHash[0:6]
    pos += 7
    return pos

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
    pos = write32(mm, pos, args.rx_baud)
    val = mm[pos]
    if args.invert_tx != None:
        val &= ~1
        val |= args.invert_tx
    if args.lock_on_first_connection != None:
        val &= ~2
        val |= (args.lock_on_first_connection << 1)
    if args.r9mm_mini_sbus != None:
        val &= ~4
        val |= (args.r9mm_mini_sbus << 2)
    mm[pos] = val
    return pos + 1

def patch_tx_params(mm, pos, args):
    pos = write32(mm, pos, args.tlm_report)
    pos = write32(mm, pos, args.fan_min_runtime)
    val = mm[pos]
    if args.uart_inverted != None:
        val &= ~1
        val |= args.uart_inverted
    if args.unlock_higher_power != None:
        val &= ~2
        val |= (args.unlock_higher_power << 1)
    mm[pos] = val
    return pos + 1

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

def patch_firmware(options, mm, pos, args):
    if options.radioChip is RadioType.SX127X and args.domain:
        mm[pos] = domain_number(args.domain)
    pos += 1

    if options.mcuType is MCUType.STM32:
        pos = patch_uid(mm, pos, args)
        if options.deviceType is DeviceType.TX:
            pos = patch_tx_params(mm, pos, args)
            if options.hasBuzzer:
                pos = patch_buzzer(mm, pos, args)
        elif options.deviceType is DeviceType.RX:
            pos = patch_rx_params(mm, pos, args)
    else:
        patch_unified(args, options)

def print_domain(radio, domain):
    if radio is RadioType.SX1280:
        if domain == 0:
            print('Regulatory Domain is ISM 2.4GHz')
        else:
            raise AssertionError('Invalid domain detected!')
    else:
        if domain == 0:
            print('Regulatory Domain is AU 915 MHz')
        if domain == 1:
            print('Regulatory Domain is FCC 915 MHz')
        if domain == 2:
            print('Regulatory Domain is EU 868 MHz')
        if domain == 3:
            print('Regulatory Domain is IN 866 MHz')
        if domain == 4:
            print('Regulatory Domain is AU 433 MHz')
        if domain == 5:
            print('Regulatory Domain is EU 433 MHz')
        else:
            raise AssertionError('Invalid domain detected!')

def print_config(options: FirmwareOptions, mm, pos):
    mcu = ['STM32', 'ESP32', 'ESP8266', 'Unknown'][options.mcuType.value]
    device = ['TX', 'RX', 'TX Backpack', 'VRx Backpack', 'Unknown', 'Unknown', 'Unknown', 'Unknown'][options.deviceType.value]
    radio = ['SX1280', 'SX127X'][options.radioChip.value]
    print(f'MCU: {mcu}, Device: {device}, Radio: {radio}')

    domain = mm[pos]
    pos += 1
    print_domain(options.radioChip, domain)

    hasUID = mm[pos]
    pos += 1
    UID = mm[pos:pos+6]
    pos += 6
    if hasUID:
        print(f'Binding phrase was used, UID = {UID[0],UID[1],UID[2],UID[3],UID[4],UID[5]}')
    else:
        print('No binding phrase set')

    if options.hasWiFi:
        (pos, val) = read32(mm, pos)
        if val == 0xFFFFFFFF:
            print('WiFi auto on is disabled')
        else:
            print(f'WiFi auto on interval = {int(val/1000)} seconds')
        (pos, ssid) = readString(mm, pos, 33)
        (pos, password) = readString(mm, pos, 65)
        print(f'WiFi SSID: {ssid}')
        print(f'WiFi Password: {password}')

    if options.deviceType is DeviceType.TX:
        (pos, tlm) = read32(mm, pos)
        (pos, fan) = read32(mm, pos)
        val = mm[pos]
        pos += 1
        uart_inverted = (val & 1) == 1
        unlock_higher_power = (val & 2) == 2
        print(f'Telemetry report interval = {tlm}ms')
        print(f'Fan minimum run time = {fan}s')
        print(f'UART_INVERTED is {uart_inverted}')
        print(f'UNLOCK_HIGHER_POWER is {unlock_higher_power}')
        if options.hasBuzzer:
            mode = mm[pos]
            pos += 1
            melody = []
            for x in range(32):
                (pos, val) = read32(mm, pos)
                if val != 0:
                    melody.append([val & 0xFFFF, (val >> 16) & 0xFFFF])
            if mode == 0:
                print('Buzzer mode quiet')
            if mode == 1:
                print('Buzzer mode beep once')
            if mode == 2:
                print('Buzzer mode play tune')
                print(melody)
            None
    elif options.deviceType is DeviceType.RX:
        (pos, baud) = read32(mm, pos)
        val = mm[pos]
        pos += 1
        invert_tx = (val & 1) == 1
        lock_on_first_connection = (val & 2) == 2
        r9mm_mini_sbus = (val & 4) == 4
        print(f'Receiver CRSF baud rate = {baud}')
        print(f'RCVR_INVERT_TX is {invert_tx}')
        print(f'LOCK_ON_FIRST_CONNECTION is {lock_on_first_connection}')
        print(f'USE_R9MM_MINI_SBUS is {r9mm_mini_sbus}')
    elif options.deviceType is DeviceType.TX_Backpack:
        None
    elif options.deviceType is DeviceType.VRx_Backpack:
        None
    return

def patch_unified(args, options):
    json_flags = {}
    if args.phrase is not None:
        json_flags['uid'] = bindingPhraseHash = [x for x in hashlib.md5(("-DMY_BINDING_PHRASE=\""+args.phrase+"\"").encode()).digest()[0:6]]
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
    if args.uart_inverted is not None:
        json_flags['uart-inverted'] = args.uart_inverted

    if args.rx_baud is not None:
        json_flags['rcvr-uart-baud'] = args.rx_baud
    if args.invert_tx is not None:
        json_flags['rcvr-invert-tx'] = args.invert_tx
    if args.lock_on_first_connection is not None:
        json_flags['lock-on-first-connection'] = args.lock_on_first_connection

    if args.domain is not None:
        json_flags['domain'] = domain_number(args.domain)

    UnifiedConfiguration.doConfiguration(
        args.file,
        JSONEncoder().encode(json_flags),
        args.target,
        'tx' if options.deviceType is DeviceType.TX else 'rx',
        '2400' if options.radioChip is RadioType.SX1280 else '900',
        '32' if options.mcuType is MCUType.ESP32 and options.deviceType is DeviceType.RX else ''
    )

def length_check(l, f):
    def x(s):
        if (len(s) > l):
            raise argparse.ArgumentTypeError(f'too long, {l} chars max')
        else:
            return s
    return x

def main():
    parser = argparse.ArgumentParser(description="Configure Binary Firmware")
    parser.add_argument('--print', action='store_true', help='Print the current configuration in the firmware')
    # Bind phrase
    parser.add_argument('--phrase', type=str, help='Your personal binding phrase')
    # WiFi Params
    parser.add_argument('--ssid', type=length_check(32, "ssid"), required=False, help='Home network SSID')
    parser.add_argument('--password', type=length_check(64, "password"), required=False, help='Home network password')
    parser.add_argument('--auto-wifi', type=int, help='Interval (in seconds) before WiFi auto starts, if no connection is made')
    parser.add_argument('--no-auto-wifi', action='store_true', help='Disables WiFi auto start if no connection is made')
    # RX Params
    parser.add_argument('--rx-baud', type=int, const=420000, nargs='?', action='store', help='The receiver baudrate talking to the flight controller')
    parser.add_argument('--invert-tx', dest='invert_tx', action='store_true', help='Invert the TX pin on the receiver, if connecting to SBUS pad')
    parser.add_argument('--no-invert-tx', dest='invert_tx', action='store_false', help='TX pin is connected to a regular UART (i.e. not an SBUS inverted pin)')
    parser.set_defaults(invert_tx=None)
    parser.add_argument('--lock-on-first-connection', dest='lock_on_first_connection', action='store_true', help='Lock RF mode on first connection')
    parser.add_argument('--no-lock-on-first-connection', dest='lock_on_first_connection', action='store_false', help='Do not lock RF mode on first connection')
    parser.set_defaults(lock_on_first_connection=None)
    parser.add_argument('--r9mm-mini-sbus', dest='r9mm_mini_sbus', action='store_true', help='Use the SBUS pin for CRSF output, not it will be inverted')
    parser.add_argument('--no-r9mm-mini-sbus', dest='r9mm_mini_sbus', action='store_false', help='Use the normal serial pins for CRSF')
    parser.set_defaults(r9mm_mini_sbus=None)
    # TX Params
    parser.add_argument('--tlm-report', type=int, const=240, nargs='?', action='store', help='The interval (in milliseconds) between telemetry packets')
    parser.add_argument('--fan-min-runtime', type=int, const=30, nargs='?', action='store', help='The minimum amount of time the fan should run for (in seconds) if it turns on')
    parser.add_argument('--uart-inverted', dest='uart_inverted', action='store_true', help='For most OpenTX based radios, this is the default')
    parser.add_argument('--no-uart-inverted', dest='uart_inverted', action='store_false', help='If your radio is T8SG V2 or you use Deviation firmware set this flag.')
    parser.set_defaults(uart_inverted=None)
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
    parser.add_argument("--port", type=str, help="SerialPort or WiFi address to flash firmware to")
    parser.add_argument("--baud", type=int, default=0, help="Baud rate for serial communication")
    parser.add_argument("--force", action='store_true', default=False, help="Force upload even if target does not match")
    parser.add_argument("--confirm", action='store_true', default=False, help="Confirm upload if a mismatched target was previously uploaded")

    #
    # Firmware file to patch/configure
    parser.add_argument("file", type=argparse.FileType("r+b"))

    args = parser.parse_args()

    with args.file as f:
        mm = mmap.mmap(f.fileno(), 0)

        options, target, pos = firmware.get_hardware(mm)
        if args.print:
            print_config(options, mm, pos)
        else:
            patch_firmware(options, mm, pos, args)
            if args.flash:
                args.target = target
                return binary_flash.upload(options, args)

if __name__ == '__main__':
    try:
        main()
    except AssertionError as e:
        print(e)
        exit(1)
