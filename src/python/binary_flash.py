#!/usr/bin/python

import argparse
from enum import Enum
import mmap

from elrs_helpers import ElrsUploadResult
import BFinitPassthrough
import ETXinitPassthrough
import serials_find
import UARTupload
import upload_via_esp8266_backpack
from external.esptool import esptool

class MCUType(Enum):
    STM32 = 0
    ESP32 = 1
    ESP8266 = 2

class UploadMethod(Enum):
    wifi = 'wifi'
    uart = 'uart'
    betaflight = 'bf'
    edgetx = 'etx'
    stlink = 'stlink'

    def __str__(self):
        return self.value

def get_hardware(mm):
    pos = mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')
    if pos == -1:
        raise AssertionError('Configuration magic not found in firmware file. Is this a 2.3 firmware?')
    pos += 8 + 2                # Skip magic & version
    hardware = mm[pos]
    target_rx = False
    if hardware & 2:            # RX target
        target_rx = True
    mcu_type = hardware >> 4    # MCU type

    pos = mm.find(b'\xBE\xEF\xCA\xFE')
    if pos == -1:
        target = ''
    else:
        target = (mm[pos+4:mm.find(b'\x00', pos+4)]).decode()

    return target_rx, mcu_type, target

def upload_wifi(args, upload_addr, isstm: bool):
    wifi_mode = 'upload'
    if args.force == True:
        wifi_mode = 'uploadforce'
    elif args.confirm == True:
        wifi_mode = 'uploadconfirm'
    if args.port:
        upload_addr = [args.port]
    print (upload_addr)
    return upload_via_esp8266_backpack.do_upload(args.file.name, wifi_mode, upload_addr, isstm, {})

def upload_stm32_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    return UARTupload.uart_upload(args.port, args.file.name, args.baud, target=args.target)

def upload_stm32_stlink(args):
    return ElrsUploadResult.Success

def upload_esp8266_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    try:
        esptool.main(['--chip', 'esp8266', '--baud', str(args.baud), '--after', 'soft_reset', 'write_flash', '0x0000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp8266_bf(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    mode = 'upload'
    if args.force == True:
        mode = 'uploadforce'
    retval = BFinitPassthrough.main(['-p', args.port, '-b', str(args.baud), '-r', args.target, '-a', mode])
    if retval != ElrsUploadResult.Success:
        return retval
    try:
        esptool.main(['--chip', 'esp8266', '--baud', str(args.baud), '--before', 'no_reset', '--after', 'soft_reset', 'write_flash', '0x0000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_uart(args):
    try:
        esptool.main(['--chip', 'esp32', '--baud', str(args.baud), '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', 'bootloader_dio_40m.bin', '0x8000', 'partitions.bin', '0xe000', 'boot_app0.bin', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_etx(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    ETXinitPassthrough.etx_passthrough_init(args.port, args.baud)
    try:
        esptool.main(['--chip', 'esp32', '--baud', str(args.baud), '--before', 'no_reset', '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', 'bootloader_dio_40m.bin', '0x8000', 'partitions.bin', '0xe000', 'boot_app0.bin', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload(target_rx, mcu_type, args):
    if args.baud == 0:
        args.baud = 460800
        if args.method == UploadMethod.betaflight:
            args.baud = 420000

    if target_rx == True:
        if mcu_type == MCUType.ESP8266.value:
            if args.method == UploadMethod.betaflight:
                return upload_esp8266_bf(args)
            elif args.method == UploadMethod.uart:
                return upload_esp8266_uart(args)
            elif args.method == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_rx', 'elrs_rx.local'], False)
        elif mcu_type == MCUType.STM32.value:
            if args.method == UploadMethod.betaflight or args.method == UploadMethod.uart:
                return upload_stm32_uart(args)
            elif args.method == UploadMethod.stlink:      # untested
                return upload_stm32_stlink(args)
    else:
        if mcu_type == MCUType.ESP32.value:
            if args.method == UploadMethod.edgetx:
                return upload_esp32_etx(args)
            elif args.method == UploadMethod.uart:
                return upload_esp32_uart(args)
            elif args.method == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_tx', 'elrs_tx.local'], False)
        elif mcu_type == MCUType.STM32.value:
            if args.method == UploadMethod.stlink:      # test
                return upload_stm32_stlink(args)
            elif args.method == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_txbp', 'elrs_txbp.local'], True)
    print("Invalid upload method for firmware")
    return ElrsUploadResult.ErrorGeneral

def main():
    parser = argparse.ArgumentParser(description="Upload Binary Firmware")

    parser.add_argument("-p", "--port", type=str,
        help="SerialPort or WiFi address to flash firmware to")
    parser.add_argument("-b", "--baud", type=int, default=0,
        help="Baud rate for serial communication")
    parser.add_argument("-m", "--method", type=UploadMethod, choices=list(UploadMethod), required=True,
        help="Upload Method")
    parser.add_argument("-f", "--force", action='store_true', default=False,
        help="Force upload even if target does not match")
    parser.add_argument("-c", "--confirm", action='store_true', default=False,
        help="Confirm upload if a mismatched target was previously uploaded")

    # Firmware file to upload
    parser.add_argument("file", type=argparse.FileType("rb"))

    args = parser.parse_args()

    with args.file as f:
        mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        target_rx, mcu_type, target = get_hardware(mm)

    args.target = target
    return upload(target_rx, mcu_type, args)

if __name__ == '__main__':
    try:
        retval = main()
    except AssertionError as e:
        print(e)
        retval = ElrsUploadResult.ErrorGeneral
    exit(retval)
