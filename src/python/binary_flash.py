#!/usr/bin/python

from enum import Enum
import shutil
import os

from elrs_helpers import ElrsUploadResult
import BFinitPassthrough
import ETXinitPassthrough
import serials_find
import UARTupload
import upload_via_esp8266_backpack
from firmware import DeviceType, FirmwareOptions, MCUType

import sys
from os.path import dirname
sys.path.append(dirname(__file__) + '/external/esptool')

from external.esptool import esptool
sys.path.append(dirname(__file__) + "/external")

import external.pystlink

class UploadMethod(Enum):
    wifi = 'wifi'
    uart = 'uart'
    betaflight = 'bf'
    edgetx = 'etx'
    stlink = 'stlink'
    stock = 'stock'
    dir = 'dir'

    def __str__(self):
        return self.value

def upload_wifi(args, options, upload_addr, isstm: bool):
    wifi_mode = 'upload'
    if args.force == True:
        wifi_mode = 'uploadforce'
    elif args.confirm == True:
        wifi_mode = 'uploadconfirm'
    if args.port:
        upload_addr = [args.port]
    if options.mcuType == MCUType.ESP8266:
        return upload_via_esp8266_backpack.do_upload('firmware.bin.gz', wifi_mode, upload_addr, isstm, {})
    else:
        return upload_via_esp8266_backpack.do_upload(args.file.name, wifi_mode, upload_addr, isstm, {})

def upload_stm32_uart(args, options):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    return UARTupload.uart_upload(args.port, args.file.name, args.baud, target=options.firmware.upper(), accept=args.accept, ignore_incorrect_target=args.force)

def upload_stm32_stlink(args, options: FirmwareOptions):
    stlink = external.pystlink.PyStlink(verbosity=1)

    flash_start = app_start = 0x08000000
    if "0x" in options.offset:
        offset = int(options.offset, 16)
    else:
        offset = int(options.offset, 10)
    app_start = flash_start + offset

    if options.bootloader is not None:
        bootloader_dir = '' if args.fdir is None else args.fdir + '/'
        stlink.program_flash(bootloader_dir + 'bootloader/' + options.bootloader, flash_start, erase=True, verify=True, initialize_comms=True)

    stlink.program_flash(args.file.name, app_start, erase=True, verify=True, initialize_comms=True)

    return ElrsUploadResult.Success

def upload_esp8266_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    try:
        cmd = ['--chip', 'esp8266', '--port', args.port, '--baud', str(args.baud), '--after', 'soft_reset', 'write_flash']
        if args.erase: cmd.append('--erase-all')
        cmd.extend(['0x0000', args.file.name])
        esptool.main(cmd)
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp8266_bf(args, options):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    mode = 'upload'
    if args.force == True:
        mode = 'uploadforce'
    retval = BFinitPassthrough.main(['-p', args.port, '-b', str(args.baud), '-r', options.firmware, '-a', mode, '--accept', args.accept])
    if retval != ElrsUploadResult.Success:
        return retval
    try:
        cmd = ['--passthrough', '--chip', 'esp8266', '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'soft_reset', '--no-stub', 'write_flash']
        if args.erase: cmd.append('--erase-all')
        cmd.extend(['0x0000', args.file.name])
        esptool.main(cmd)
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    try:
        dir = os.path.dirname(args.file.name)
        cmd = ['--chip', args.platform.replace('-', ''), '--port', args.port, '--baud', str(args.baud), '--after', 'hard_reset', 'write_flash']
        if args.erase: cmd.append('--erase-all')
        start_addr = '0x0000' if args.platform.startswith('esp32-') else '0x1000'
        cmd.extend(['-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', start_addr, os.path.join(dir, 'bootloader.bin'), '0x8000', os.path.join(dir, 'partitions.bin'), '0xe000', os.path.join(dir, 'boot_app0.bin'), '0x10000', args.file.name])
        esptool.main(cmd)
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_etx(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    ETXinitPassthrough.etx_passthrough_init(args.port, args.baud)
    try:
        dir = os.path.dirname(args.file.name)
        cmd = ['--chip', args.platform.replace('-', ''), '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'hard_reset', 'write_flash']
        if args.erase: cmd.append('--erase-all')
        start_addr = '0x0000' if args.platform.startswith('esp32-') else '0x1000'
        cmd.extend(['-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', start_addr, os.path.join(dir, 'bootloader.bin'), '0x8000', os.path.join(dir, 'partitions.bin'), '0xe000', os.path.join(dir, 'boot_app0.bin'), '0x10000', args.file.name])
        esptool.main(cmd)
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_bf(args, options):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    mode = 'upload'
    if args.force == True:
        mode = 'uploadforce'
    retval = BFinitPassthrough.main(['-p', args.port, '-b', str(args.baud), '-r', options.firmware, '-a', mode])
    if retval != ElrsUploadResult.Success:
        return retval
    try:
        esptool.main(['--passthrough', '--chip', args.platform.replace('-', ''), '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_dir(mcuType, args):
    if mcuType == MCUType.STM32:
        if args.flash == UploadMethod.stock:
            shutil.copy2(args.file.name, os.path.join(args.out, 'firmware.elrs'))
        else:
            shutil.copy2(args.file.name, args.out)
    if mcuType == MCUType.ESP8266:
        shutil.copy2('firmware.bin.gz', os.path.join(args.out, 'firmware.bin.gz'))
    elif mcuType == MCUType.ESP32:
        shutil.copy2(args.file.name, args.out)

def upload(options: FirmwareOptions, args):
    if args.baud == 0:
        args.baud = 460800
        if args.flash == UploadMethod.betaflight:
            args.baud = 420000

    if args.flash == UploadMethod.dir or args.flash == UploadMethod.stock:
        return upload_dir(options.mcuType, args)
    elif options.deviceType == DeviceType.RX:
        if options.mcuType == MCUType.ESP8266:
            if args.flash == UploadMethod.betaflight:
                return upload_esp8266_bf(args, options)
            elif args.flash == UploadMethod.uart:
                return upload_esp8266_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, options, ['elrs_rx', 'elrs_rx.local'], False)
        elif options.mcuType == MCUType.ESP32:
            if args.flash == UploadMethod.betaflight:
                return upload_esp32_bf(args, options)
            elif args.flash == UploadMethod.uart:
                return upload_esp32_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, options, ['elrs_rx', 'elrs_rx.local'], False)
        elif options.mcuType == MCUType.STM32:
            if args.flash == UploadMethod.betaflight or args.flash == UploadMethod.uart:
                return upload_stm32_uart(args, options)
            elif args.flash == UploadMethod.stlink:      # untested
                return upload_stm32_stlink(args, options)
    else:
        if options.mcuType == MCUType.ESP8266:
            if args.flash == UploadMethod.uart:
                return upload_esp8266_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, options, ['elrs_tx', 'elrs_tx.local'], False)
        elif options.mcuType == MCUType.ESP32:
            if args.flash == UploadMethod.edgetx:
                return upload_esp32_etx(args)
            elif args.flash == UploadMethod.uart:
                return upload_esp32_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, options, ['elrs_tx', 'elrs_tx.local'], False)
        elif options.mcuType == MCUType.STM32:
            if args.flash == UploadMethod.stlink:      # test
                return upload_stm32_stlink(args, options)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, options, ['elrs_txbp', 'elrs_txbp.local'], True)
    print("Invalid upload method for firmware")
    return ElrsUploadResult.ErrorGeneral
