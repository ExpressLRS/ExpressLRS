#!/usr/bin/python

from enum import Enum
from typing import NamedTuple

class MCUType(Enum):
    STM32 = 0
    ESP32 = 1
    ESP8266 = 2

class DeviceType(Enum):
    TX = 0
    RX = 1
    TX_Backpack = 2
    VRx_Backpack = 3

class RadioType(Enum):
    SX127X = 0
    SX1280 = 1

class FirmwareOptions(NamedTuple):
    hasWiFi: bool
    hasBuzzer: bool
    mcuType: MCUType
    deviceType: DeviceType
    radioChip: RadioType

def find_patch_location(mm):
    return mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')

def get_hardware(mm):
    pos = mm.find(b'\xBE\xEF\xCA\xFE')
    if pos == -1:
        target = ''
    else:
        target = (mm[pos+4:mm.find(b'\x00', pos+4)]).decode()

    pos = mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')
    if pos == -1:
        raise AssertionError('Configuration magic not found in firmware file. Is this a 3.x firmware?')
    pos += 8 + 2                    # Skip magic & version
    hardware = mm[pos]

    options = FirmwareOptions(
        True if hardware & 1 == 1 else False,
        True if hardware & 2 == 2 else False,
        MCUType((hardware >> 2) & 3),
        DeviceType((hardware >> 4) & 7),
        RadioType((hardware >> 7) & 1)
    )
    pos += 1                        # Skip the hardware flag

    return options, target, pos
