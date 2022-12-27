#!/usr/bin/python

from enum import Enum
import re

from elrs_helpers import ElrsUploadResult
import BFinitPassthrough
import ETXinitPassthrough
import serials_find
import UARTupload
import upload_via_esp8266_backpack
import stlink
from firmware import DeviceType, FirmwareOptions, MCUType
from external.esptool import esptool

bootloader_args = {
    'DIY_2400_RX_STM32_CCG_Nano_v0_5': {'offset': 0x4000, 'bootloader': 'sx1280_rx_nano_pcb_v0.5_bootloader.bin'},
    'Frsky_TX_R9M': {'offset': 0x4000, 'bootloader': 'r9m_bootloader.bin'},
    'Frsky_TX_R9M_LITE': {'offset': 0x4000, 'bootloader': 'r9m_bootloader.bin'},
    'Frsky_TX_R9M_LITE_PRO': {'offset': 0x8000, 'bootloader': 'r9m_lite_pro_bootloader.bin'},
    'Frsky_RX_R9MM_R9MINI': {'offset': 0x8000, 'bootloader': 'r9mm_bootloader.bin'},
    'Frsky_RX_R9SLIM': {'offset': 0x8000, 'bootloader': 'r9slim_no_btn_bootloader.bin'},
    'Frsky_RX_R9SLIMPLUS': {'offset': 0x8000, 'bootloader': 'r9slim_plus_bootloader.bin'},
    'Frsky_RX_R9SLIMPLUS_OTA': {'offset': 0x8000, 'bootloader': 'r9slim_plus_ota_bootloader.bin'},
    'Frsky_RX_R9MX': {'offset': 0x8000, 'bootloader': 'r9mx_bootloader.bin'},
    'Jumper_RX_R900MINI': {'offset': 0x8000, 'bootloader': 'jumper_r900_bootloader.bin'},

    'HappyModel_TX_ES915TX': {'offset': 0x4000, 'bootloader': 'r9m_bootloader.bin'},
    'HappyModel_RX_ES915RX': {'offset': 0x8000, 'bootloader': 'r9mm_bootloader.bin'},
    'HappyModel_PP_2400_RX': {'offset': 0x4000, 'bootloader': 'sx1280_rx_nano_pcb_v0.5_bootloader.bin'},

    'GHOST_2400_TX': {'offset': 0x4000, 'bootloader': 'ghost/ghost_tx_bootloader.bin'},
    'GHOST_2400_TX_LITE': {'offset': 0x4000, 'bootloader': 'ghost/ghost_tx_bootloader.bin'},
    'GHOST_ATTO_2400_RX': {'offset': 0x4000, 'bootloader': 'ghost/ghost_atto_bootloader.bin'},

    'NamimnoRC_VOYAGER_900_TX': {'offset': 0x4000, 'bootloader': 'namimnorc/tx/namimnorc_tx_bootloader.bin'},
    'NamimnoRC_VOYAGER_900_RX': {'offset': 0x8000, 'bootloader': 'namimnorc/rx/voyager_900_bootloader.bin'},
    'NamimnoRC_FLASH_2400_TX': {'offset': 0x4000, 'bootloader': 'namimnorc/tx/namimnorc_tx_bootloader.bin'},
    'NamimnoRC_FLASH_2400_RX': {'offset': 0x8000, 'bootloader': 'namimnorc/rx/flash_2400_bootloader.bin'},

    'FM30_TX': {'offset': 0x1000, 'bootloader': 'fm30_bootloader.bin'},
    'FM30_RX_MINI': {'offset': 0x4000, 'bootloader': 'fm30_mini_bootloader.bin'},
    'FM30_RX_MINI_AS_TX': {'offset': 0x4000, 'bootloader': 'fm30_mini_bootloader.bin'},
}

class UploadMethod(Enum):
    wifi = 'wifi'
    uart = 'uart'
    betaflight = 'bf'
    edgetx = 'etx'
    stlink = 'stlink'

    def __str__(self):
        return self.value

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
    return UARTupload.uart_upload(args.port, args.file.name, args.baud, target=args.target, accept=args.accept)

def upload_stm32_stlink(args):
    bl_args = bootloader_args[re.sub('_via_.*', '', args.target)]
    stlink.on_upload([args.filename], None, {'UPLOAD_FLAGS': [f'BOOTLOADER=bootloader/${bl_args["file"]}', f'VECT_OFFSET=${bl_args["offset"]}']})
    return ElrsUploadResult.Success

def upload_esp8266_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    try:
        esptool.main(['--chip', 'esp8266', '--port', args.port, '--baud', str(args.baud), '--after', 'soft_reset', 'write_flash', '0x0000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp8266_bf(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    mode = 'upload'
    if args.force == True:
        mode = 'uploadforce'
    retval = BFinitPassthrough.main(['-p', args.port, '-b', str(args.baud), '-r', args.target, '-a', mode, '--accept', args.accept])
    if retval != ElrsUploadResult.Success:
        return retval
    try:
        esptool.main(['--chip', 'esp8266', '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'soft_reset', 'write_flash', '0x0000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_uart(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    try:
        esptool.main(['--chip', 'esp32', '--port', args.port, '--baud', str(args.baud), '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', 'bootloader_dio_40m.bin', '0x8000', 'partitions.bin', '0xe000', 'boot_app0.bin', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_etx(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    ETXinitPassthrough.etx_passthrough_init(args.port, args.baud)
    try:
        esptool.main(['--chip', 'esp32', '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', 'bootloader_dio_40m.bin', '0x8000', 'partitions.bin', '0xe000', 'boot_app0.bin', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload_esp32_bf(args):
    if args.port == None:
        args.port = serials_find.get_serial_port()
    mode = 'upload'
    if args.force == True:
        mode = 'uploadforce'
    retval = BFinitPassthrough.main(['-p', args.port, '-b', str(args.baud), '-r', args.target, '-a', mode])
    if retval != ElrsUploadResult.Success:
        return retval
    try:
        esptool.main(['--chip', 'esp32', '--port', args.port, '--baud', str(args.baud), '--before', 'no_reset', '--after', 'hard_reset', 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', 'bootloader_dio_40m.bin', '0x8000', 'partitions.bin', '0xe000', 'boot_app0.bin', '0x10000', args.file.name])
    except:
        return ElrsUploadResult.ErrorGeneral
    return ElrsUploadResult.Success

def upload(options: FirmwareOptions, args):
    if args.baud == 0:
        args.baud = 460800
        if args.flash == UploadMethod.betaflight:
            args.baud = 420000

    if options.deviceType == DeviceType.RX:
        if options.mcuType == MCUType.ESP8266:
            if args.flash == UploadMethod.betaflight:
                return upload_esp8266_bf(args)
            elif args.flash == UploadMethod.uart:
                return upload_esp8266_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_rx', 'elrs_rx.local'], False)
        elif options.mcuType == MCUType.ESP32:
            if args.flash == UploadMethod.betaflight:
                return upload_esp32_bf(args)
            elif args.flash == UploadMethod.uart:
                return upload_esp32_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_rx', 'elrs_rx.local'], False)
        elif options.mcuType == MCUType.STM32:
            if args.flash == UploadMethod.betaflight or args.flash == UploadMethod.uart:
                return upload_stm32_uart(args)
            elif args.flash == UploadMethod.stlink:      # untested
                return upload_stm32_stlink(args)
    else:
        if options.mcuType == MCUType.ESP32:
            if args.flash == UploadMethod.edgetx:
                return upload_esp32_etx(args)
            elif args.flash == UploadMethod.uart:
                return upload_esp32_uart(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_tx', 'elrs_tx.local'], False)
        elif options.mcuType == MCUType.STM32:
            if args.flash == UploadMethod.stlink:      # test
                return upload_stm32_stlink(args)
            elif args.flash == UploadMethod.wifi:
                return upload_wifi(args, ['elrs_txbp', 'elrs_txbp.local'], True)
    print("Invalid upload method for firmware")
    return ElrsUploadResult.ErrorGeneral
