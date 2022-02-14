#!/usr/bin/python

import argparse
import mmap
import hashlib
from enum import Enum
import melodyparser

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

def find_patch_location(mm):
    return mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')

def write32(mm, pos, val):
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
    interval = args.auto_wifi * 1000
    pos = write32(interval)
    mm[pos:pos + len(args.ssid)] = args.ssid.encode()
    pos += 33           # account for the nul
    mm[pos:pos + len(args.password)] = args.password.encode()
    pos += 65           # account for the nul
    return pos

def patch_rx_params(mm, pos, args):
    pos = write32(mm, pos, args.rx_baud)
    mm[pos] = args.invert_tx | (args.lock_on_first_connection << 1)
    pos += 1
    return pos

def patch_tx_params(mm, pos, args):
    pos = write32(mm, pos, args.tlm_report)
    flags = args.sync_on_arm << 0
    flags |= args.uart_inverted << 1
    flags |= args.unlock_higher_power << 2
    mm[pos] = flags
    pos += 1
    return pos

def patch_buzzer(mm, pos, args):
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
    pos += 1

    melodyArray = melodyparser.parseToArray(melody)
    mpos = 0
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

def FREQ_HZ_TO_REG_VAL(freq):
    return int(freq/61.03515625)

def generate_domain(mm, pos, count, init, step):
    pos = write32(mm, pos, count)
    val = init
    for x in range(count):
        pos = write32(mm, pos, FREQ_HZ_TO_REG_VAL(val))
        val += step

def patch_domain(mm, args):
    pos = mm.find(b'\xF0\x0D\xD0\xBE')
    pos += 4                    # skip magic
    if args.domain == RegulatoryDomain.eu_433:
        pos = write32(mm, pos, 3)
        pos = write32(mm, pos, FREQ_HZ_TO_REG_VAL(433100000))
        pos = write32(mm, pos, FREQ_HZ_TO_REG_VAL(433925000))
        pos = write32(mm, pos, FREQ_HZ_TO_REG_VAL(434450000))
    elif args.domain == RegulatoryDomain.au_433:
        generate_domain(mm, pos, 3, 433420000, 500000)
    elif args.domain == RegulatoryDomain.in_866:
        generate_domain(mm, pos, 4, 865375000, 525000)
    elif args.domain == RegulatoryDomain.eu_868:
        generate_domain(mm, pos, 13, 863275000, 525000)
    elif args.domain == RegulatoryDomain.au_915:
        generate_domain(mm, pos, 20, 915500000, 600000)
    elif args.domain == RegulatoryDomain.fcc_915:
        generate_domain(mm, pos, 40, 903500000, 600000)

def patch_firmware(mm, pos, args):
    pos += 8 + 2                # Skip magic & version
    hardware = mm[pos]
    _hasWiFi = hardware & 1
    _hasBuzzer = hardware & 2
    _mcuType = (hardware >> 2) & 3
    _deviceType = (hardware >> 4) & 7
    _radioChip = (hardware >> 7) & 1
    pos += 1                    # Skip the hardware flag

    pos = patch_uid(mm, pos, args)
    if _hasWiFi:                # Has WiFi (i.e. ESP8266 or ESP32)
        pos = patch_wifi(mm, pos, args)
    if _deviceType == 0:        # TX target
        pos = patch_tx_params(mm, pos, args)
        if _hasBuzzer:          # Has a Buzzer
            pos = patch_buzzer(mm, pos, args)
    if _deviceType == 1:        # RX target
        pos = patch_rx_params(mm, pos, args)

    if _radioChip == 0:         # SX127X
        patch_domain(mm, args)

def print_config(mm, pos):
    pos += 8 + 2                # Skip magic & version
    hardware = mm[pos]
    _hasWiFi = hardware & 1
    _hasBuzzer = hardware & 2
    _mcuType = (hardware >> 2) & 3
    _deviceType = (hardware >> 4) & 7
    _radioChip = (hardware >> 7) & 1
    pos += 1                    # Skip the hardware flag

    mcu = ['STM32', 'ESP32', 'ESP8266', 'Unknown'][_mcuType]
    device = ['TX', 'RX', 'TX Backpack', 'VRx Backpack', 'Unknown', 'Unknown', 'Unknown', 'Unknown'][_deviceType]
    radio = ['SX1280', 'SX127X'][_radioChip]
    print(f'MCU: {mcu}, Device: {device}, Radio: {radio}')

    hasUID = mm[pos]
    pos += 1
    UID = mm[pos:pos+6]
    pos += 6
    if hasUID:
        print(f'Binding phrase was used, UID = {UID[0],UID[1],UID[2],UID[3],UID[4],UID[5]}')
    else:
        print('No binding phrase set')

    if _hasWiFi:
        (pos, val) = read32(mm, pos)
        if val == 0xFFFFFFFF:
            print('WiFi auto on is disabled')
        else:
            print(f'WiFi auto on interval = {int(val/1000)} seconds')
        (pos, ssid) = readString(mm, pos, 33)
        (pos, password) = readString(mm, pos, 65)
        print(f'WiFi SSID: {ssid}')
        print(f'WiFi Password: {password}')

    if _deviceType == 0:    # TX
        (pos, tlm) = read32(mm, pos)
        val = mm[pos]
        pos += 1
        no_sync_on_arm = (val & 1) == 1
        uart_inverted = (val & 2) == 2
        unlock_higher_power = (val & 4) == 4
        print(f'Telemetry report interval = {tlm}ms')
        print(f'NO_SYNC_ON_ARM is {no_sync_on_arm}')
        print(f'UART_INVERTED is {uart_inverted}')
        print(f'UNLOCK_HIGHER_POWER is {unlock_higher_power}')
        if _hasBuzzer:
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
    elif _deviceType == 1:  # RX
        (pos, baud) = read32(mm, pos)
        val = mm[pos]
        pos += 1
        invert_tx = (val & 1) == 1
        lock_on_first_connection = (val & 2) == 2
        print(f'Receiver CRSF baud rate = {baud}')
        print(f'RCVR_INVERT_TX is {invert_tx}')
        print(f'LOCK_ON_FIRST_CONNECTION is {lock_on_first_connection}')
    elif _deviceType == 2:  # TXBP
        None
    elif _deviceType == 3:  # VRX
        None

    return

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
    parser.add_argument('--ssid', type=length_check(32, "ssid"), required=False, default="", help='Home network SSID')
    parser.add_argument('--password', type=length_check(64, "password"), required=False, default="", help='Home network password')
    parser.add_argument('--auto-wifi', type=int, default=60, help='Interval (in seconds) before WiFi auto starts, if no connection is made')
    # RX Params
    parser.add_argument('--rx-baud', type=int, default=420000, help='The receiver baudrate talking to the flight controller')
    parser.add_argument('--invert-tx', dest='invert_tx', action='store_true', help='Invert the TX pin on the receiver, if connecting to SBUS pad')
    parser.add_argument('--no-invert-tx', dest='invert_tx', action='store_false', help='Invert the TX pin on the receiver, if connecting to SBUS pad')
    parser.set_defaults(invert_tx=False)
    parser.add_argument('--lock-on-first-connection', dest='lock_on_first_connection', action='store_true', help='Lock RF mode on first connection')
    parser.add_argument('--no-lock-on-first-connection', dest='lock_on_first_connection', action='store_false', help='Lock RF mode on first connection')
    parser.set_defaults(lock_on_first_connection=True)
    # TX Params
    parser.add_argument('--tlm-report', type=int, default=320, help='The interval (in milliseconds) between telemetry packets')
    parser.add_argument('--opentx-sync', dest='opentx_sync', action='store_true', help='Send sync packets to OpenTx (crossfire-shot)')
    parser.add_argument('--no-opentx-sync', dest='opentx_sync', action='store_false', help='Send sync packets to OpenTx (crossfire-shot)')
    parser.set_defaults(opentx_sync=True)
    parser.add_argument('--opentx-sync-autotune', dest='opentx_sync_autotune', action='store_true', help='Auto tunes the offsets sent to OpenTX for sync packets')
    parser.add_argument('--no-opentx-sync-autotune', dest='opentx_sync_autotune', action='store_false', help='Auto tunes the offsets sent to OpenTX for sync packets')
    parser.set_defaults(opentx_sync_autotune=False)
    parser.add_argument('--sync-on-arm', dest='sync_on_arm', action='store_true', help='Send sync packets to the RX when armed')
    parser.add_argument('--no-sync-on-arm', dest='sync_on_arm', action='store_false', help='Do not send sync packets to the RX when armed')
    parser.set_defaults(sync_on_arm=True)
    parser.add_argument('--uart-inverted', dest='uart_inverted', action='store_true', help='If your radio is T8SG V2 or you use Deviation firmware set this to False.')
    parser.add_argument('--no-uart-inverted', dest='uart_inverted', action='store_false', help='If your radio is T8SG V2 or you use Deviation firmware set this to False.')
    parser.set_defaults(uart_inverted=True)
    parser.add_argument('--unlock-higher-power', dest='unlock_higher_power', action='store_true', help='')
    parser.add_argument('--no-unlock-higher-power', dest='unlock_higher_power', action='store_false', help='')
    parser.set_defaults(unlock_higher_power=False)
    # Buzzer
    parser.add_argument('--buzzer-mode', type=BuzzerMode, choices=list(BuzzerMode), default=BuzzerMode.default, help='Which buzzer mode to use, if there is a buzzer')
    parser.add_argument('--buzzer-melody', type=str, default="", help='If the mode is "custom", then this is the tune')
    # Regulatory domain
    parser.add_argument('--domain', type=RegulatoryDomain, choices=list(RegulatoryDomain), default=RegulatoryDomain.fcc_915, help='For SX127X based device, which regulatory domain is being used')

    #
    # Firmware file to patch/configure
    parser.add_argument("file", type=argparse.FileType("r+b"))

    args = parser.parse_args()


    with args.file as f:
        mm = mmap.mmap(f.fileno(), 0)
        pos = find_patch_location(mm)
        if pos == -1:
            raise AssertionError('Configuration magic not found in firmware file. Is this a 2.3 firmware?')
        if args.print:
            print_config(mm, pos)
        else:
            patch_firmware(mm, pos, args)

if __name__ == '__main__':
    try:
        main()
    except AssertionError as e:
        print(e)
        exit(1)
