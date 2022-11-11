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
    luaName: str
    bootloader: str
    offset: int

def find_patch_location(mm):
    return mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')

def get_hardware(mm):
    pos = mm.find(b'\xBE\xEF\xBA\xBE\xCA\xFE\xF0\x0D')
    if pos != -1:
        pos += 8 + 2                    # Skip magic & version

    return pos
