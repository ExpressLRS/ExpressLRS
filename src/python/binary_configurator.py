#!/usr/bin/python

import argparse
import mmap
import hashlib
from enum import Enum
import melodyparser

class BuzzerMode(Enum):
    quiet = 'quiet'
    one = 'one-beep'
    none = 'no-tune'
    default = 'default-tune'
    custom = 'custom-tune'

    def __str__(self):
        return self.value

def find_patch_location(mm):
    return mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')

def patch_uid(mm, pos, args):
    bindingPhraseHash = hashlib.md5(("-DMY_BINDING_PHRASE=\""+args.phrase+"\"").encode()).digest()
    mm[pos:pos + 6] = bindingPhraseHash[0:6]
    pos += 6
    return pos

def patch_wifi(mm, pos, args):
    interval = args.auto_wifi * 1000
    mm[pos] = interval & 0xFF
    mm[pos + 1] = interval >> 8
    pos += 2
    mm[pos:pos + len(args.ssid)] = args.ssid.encode()
    pos += 33           # account for the nul
    mm[pos:pos + len(args.password)] = args.password.encode()
    pos += 65           # account for the nul
    return pos

def patch_rx_params(mm, pos, args):
    mm[pos + 0] = args.rx_baud >> 24 & 0xFF
    mm[pos + 1] = args.rx_baud >> 16 & 0xFF
    mm[pos + 2] = args.rx_baud >> 8 & 0xFF
    mm[pos + 3] = args.rx_baud >> 0 & 0xFF
    pos += 4
    mm[pos] = args.invert_tx | (args.lock_on_first_connection << 1)
    pos += 1
    return pos

def patch_tx_params(mm, pos, args):
    mm[pos + 0] = args.tlm_report >> 24 & 0xFF
    mm[pos + 1] = args.tlm_report >> 16 & 0xFF
    mm[pos + 2] = args.tlm_report >> 8 & 0xFF
    mm[pos + 3] = args.tlm_report >> 0 & 0xFF
    pos += 4
    flags = args.sync_on_arm << 0
    flags |= args.uart_inverted << 1
    flags |= args.unlock_higher_power << 2
    mm[pos] = flags
    pos += 1
    return pos

def patch_buzzer(mm, pos, args):
    if args.buzzer_mode == BuzzerMode.quiet:
        mm[pos] = 0
    if args.buzzer_mode == BuzzerMode.quiet:
        mm[pos] = 1
    if args.buzzer_mode == BuzzerMode.quiet:
        mm[pos] = 2
    if args.buzzer_mode == BuzzerMode.quiet:
        mm[pos] = 3
    if args.buzzer_mode == BuzzerMode.custom:
        mm[pos] = 4
    pos += 1

    melody = melodyparser.parse(args.buzzer_melody)
    print(melody)
    melody = melodyparser.parseToArray(args.buzzer_melody)
    mpos = 0
    for element in melody:
        if mpos == 32*4:
            print("Melody truncated at 32 tones")
            break
        mm[pos+mpos+0] = (int(element[0]) >> 8) & 0xFF
        mm[pos+mpos+1] = (int(element[0]) >> 0) & 0xFF
        mm[pos+mpos+2] = (int(element[1]) >> 8) & 0xFF
        mm[pos+mpos+3] = (int(element[1]) >> 0) & 0xFF
        mpos += 4
    pos += 32*4     # 32 notes 2 bytes tone, 2 bytes duration
    return pos

def patch_firmware(mm, pos, args):
    pos += 8 + 2            # Skip magic & version
    hardware = mm[pos]
    pos += 1                # Skip the hardware flag
    pos = patch_uid(mm, pos, args)
    if hardware & 1:        # Has WiFi (i.e. ESP8266 or ESP32)
        pos = patch_wifi(mm, pos, args)
    if hardware & 2:        # RX target
        pos = patch_rx_params(mm, pos, args)
    else:                   # TX target
        pos = patch_tx_params(mm, pos, args)
        if hardware & 4:    # Has a Buzzer
            pos = patch_buzzer(mm, pos, args)
    return pos

def length_check(l, f):
    def x(s):
        if (len(s) > l):
            raise argparse.ArgumentTypeError(f'too long, {l} chars max')
        else:
            return s
    return x

def main():
    parser = argparse.ArgumentParser(description="Configure Binary Firmware")
    parser.add_argument('--phrase', type=str, required=True, help='Your personal binding phrase')
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

    # Firmware file to patch/configure
    parser.add_argument("file", type=argparse.FileType("r+b"))

    args = parser.parse_args()


    with args.file as f:
        mm = mmap.mmap(f.fileno(), 0)
        pos = find_patch_location(mm)
        if pos == -1:
            raise AssertionError('Configuration magic not found in firmware file. Is this a 2.3 firmware?')
        patch_firmware(mm, pos, args)

if __name__ == '__main__':
    try:
        main()
    except AssertionError as e:
        print(e)
        exit(1)

