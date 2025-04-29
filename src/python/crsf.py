#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import serial
import serials_find
import threading


def calc_crc8(payload, poly=0xD5):
    crc = 0
    for data in payload:
        crc ^= data
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ poly
            else:
                crc = crc << 1
    return crc & 0xFF


def device_name(device: int):
    if device == 0x00: return 'Broadcast address'
    if device == 0x0E: return 'Cloud'
    if device == 0x10: return 'USB Device'
    if device == 0x12: return 'Bluetooth Module/WiFi'
    if device == 0x13: return 'Wi-Fi receiver (mobile game/simulator)'
    if device == 0x14: return 'Video Receiver'
    if device == 0x80: return 'OSD / TBS CORE PNP PRO'
    if device == 0x90: return 'ESC 1'
    if device == 0x91: return 'ESC 2'
    if device == 0x92: return 'ESC 3'
    if device == 0x93: return 'ESC 4'
    if device == 0x94: return 'ESC 5'
    if device == 0x95: return 'ESC 6'
    if device == 0x96: return 'ESC 7'
    if device == 0x97: return 'ESC 8'
    if device == 0xC0: return 'Voltage/ Current Sensor / PNP PRO digital current sensor'
    if device == 0xC2: return 'GPS / PNP PRO GPS'
    if device == 0xC4: return 'TBS Blackbox'
    if device == 0xC8: return 'Flight controller'
    if device == 0xCC: return 'Race tag'
    if device == 0xCE: return 'VTX'
    if device == 0xEA: return 'Remote Control'
    if device == 0xEC: return 'R/C Receiver / Crossfire Rx'
    if device == 0xEE: return 'R/C Transmitter Module / Crossfire Tx'
    return None


next_chunk = 0
data = b''


def process(command: bytes):
    if command[0] == 0x14: # Link Stats
        None
    elif command[0] == 0x16: # RC Packet
        None
    elif command[0] >= 0x28:
        type = command[0]
        print('Type: ' + hex(type))
        print('Dest: ' + device_name(command[1]))
        print('Orig: ' + device_name(command[2]))
        command = command[3:]
        if type == 0x29: # Parameter Device Info
            zero = command.index(0)
            print('Name: ' + str(command[:zero]))
            command = command[zero+1:]
            serial_number = command[:4]
            command = command[4:]
            print('Serial Number: ' + str(serial_number))
            hardware_id = command[:4]
            print('Hardware ID: ' + str(hardware_id))
            command = command[4:]
            software_id = command[:4]
            print('Software ID: ' + str(software_id))
            command = command[4:]
            total_params = command[0]
            print('Total params: ' + str(total_params))
            command = command[1:]
            parameter_version = command[0]
            print('Parameter Version: ' + str(parameter_version))
            command = command[1:]
        elif type == 0x2B:
            global data, next_chunk
            param = command[0]
            print('Param: ', param)
            chunks_remaining = command[1]
            print('Chunks remaining: ', chunks_remaining)
            data = data + command[2:]
            if chunks_remaining != 0:
                read_param(s, param, next_chunk)
            else:
                print('Data: ', data)
        else:
            print(command)
    else:
        print(command)


def reader(s: serial.Serial):
    starting = False
    count = 0
    command = bytes()
    while True:
        b = s.read()
        if len(b) != 0:
            if count:
                count -= 1
                if count == 0:
                    if calc_crc8(command) == ord(b):
                        process(command)
                    else:
                        s.flushInput()
                else:
                    command += b
            elif starting:
                starting = False
                if ord(b) > 62:
                    s.flushInput()
                else:
                    count = ord(b)
                    command = bytes()
            else:
                starting = True


def send(s: serial.Serial, sync: int, line: bytes):
    s.write(bytes([sync, 1 + len(line)]) + line + bytes([calc_crc8(line)]))


def read_param(s: serial.Serial, param: int, chunk: int):
    print('read_param', param, chunk)
    global next_chunk, data
    next_chunk = chunk + 1
    if chunk == 0:
        data = b''
    send(s, 0xc8, b'\x2C\xEE\xC8' + bytes([param, chunk]))


if __name__ == '__main__':
    port = serials_find.get_serial_port()
    s = serial.Serial(port=port, baudrate=420000,
                      bytesize=8, parity='N', stopbits=1,
                      timeout=0.1, xonxoff=0, rtscts=0)

    threading.Thread(target=reader, args=(s,)).start()

    for line in sys.stdin:
        if line.upper().startswith('B'):
            send(s, 0xc8, b'\x32\xEC\xC8\x10\x01')
        if line.upper().startswith('D'):
            send(s, 0xc8, b'\x28\x00\xC8')
        if line.upper().startswith('P'):
            param = int(line[1:])
            read_param(s, param, 0)
