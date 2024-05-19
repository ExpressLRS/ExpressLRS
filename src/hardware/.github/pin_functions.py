INPUT = 1
OUTPUT = 2
ADC = 4
DAC = 8

mcu_pin_functions = {
    "esp8285": {
        0: INPUT | OUTPUT,
        1: INPUT | OUTPUT,
        2: INPUT | OUTPUT,
        3: INPUT | OUTPUT,
        4: INPUT | OUTPUT,
        5: INPUT | OUTPUT,
        9: INPUT | OUTPUT,
        10: INPUT | OUTPUT,
        12: INPUT | OUTPUT,
        13: INPUT | OUTPUT,
        14: INPUT | OUTPUT,
        15: INPUT | OUTPUT,
        16: INPUT | OUTPUT,
        17: ADC
    },
    "esp32": {
        0: INPUT | OUTPUT | ADC,
        1: INPUT | OUTPUT,
        2: INPUT | OUTPUT | ADC,
        3: INPUT | OUTPUT,
        4: INPUT | OUTPUT | ADC,
        5: INPUT | OUTPUT,
        9: INPUT | OUTPUT,
        10: INPUT | OUTPUT,
        12: INPUT | OUTPUT | ADC,
        13: INPUT | OUTPUT | ADC,
        14: INPUT | OUTPUT | ADC,
        15: INPUT | OUTPUT | ADC,
        16: INPUT | OUTPUT,
        17: INPUT | OUTPUT,
        18: INPUT | OUTPUT,
        19: INPUT | OUTPUT,
        21: INPUT | OUTPUT,
        22: INPUT | OUTPUT,
        23: INPUT | OUTPUT,
        25: INPUT | OUTPUT | ADC | DAC,
        26: INPUT | OUTPUT | ADC | DAC,
        27: INPUT | OUTPUT | ADC,
        32: INPUT | OUTPUT | ADC,
        33: INPUT | OUTPUT | ADC,
        34: INPUT | ADC,
        35: INPUT | ADC,
        36: INPUT | ADC,
        37: INPUT | ADC,
        38: INPUT | ADC,
        39: INPUT | ADC
    },
    "esp32-s3": {
        0: INPUT | OUTPUT,
        1: INPUT | OUTPUT | ADC,
        2: INPUT | OUTPUT | ADC,
        3: INPUT | OUTPUT | ADC,
        4: INPUT | OUTPUT | ADC,
        5: INPUT | OUTPUT | ADC,
        6: INPUT | OUTPUT | ADC,
        7: INPUT | OUTPUT | ADC,
        8: INPUT | OUTPUT | ADC,
        9: INPUT | OUTPUT | ADC,
        10: INPUT | OUTPUT | ADC,
        11: INPUT | OUTPUT | ADC,
        12: INPUT | OUTPUT | ADC,
        13: INPUT | OUTPUT | ADC,
        14: INPUT | OUTPUT | ADC,
        15: INPUT | OUTPUT | ADC,
        16: INPUT | OUTPUT | ADC,
        17: INPUT | OUTPUT | ADC,
        18: INPUT | OUTPUT | ADC,
        19: INPUT | OUTPUT | ADC,
        20: INPUT | OUTPUT | ADC,
        21: INPUT | OUTPUT,
        # 26: INPUT | OUTPUT, # Used for flash
        # 27: INPUT | OUTPUT, # Used for flash
        # 28: INPUT | OUTPUT, # Used for flash
        # 29: INPUT | OUTPUT, # Used for flash
        # 30: INPUT | OUTPUT, # Used for flash
        # 31: INPUT | OUTPUT, # Used for flash
        # 32: INPUT | OUTPUT, # Used for flash
        33: INPUT | OUTPUT,
        34: INPUT | OUTPUT,
        35: INPUT | OUTPUT,
        36: INPUT | OUTPUT,
        37: INPUT | OUTPUT,
        38: INPUT | OUTPUT,
        39: INPUT | OUTPUT,
        40: INPUT | OUTPUT,
        41: INPUT | OUTPUT,
        42: INPUT | OUTPUT,
        43: INPUT | OUTPUT,
        44: INPUT | OUTPUT,
        45: INPUT | OUTPUT,
        46: INPUT | OUTPUT,
        47: INPUT | OUTPUT,
        48: INPUT | OUTPUT
    },
    "esp32-c3": {
        0: INPUT | OUTPUT | ADC,
        1: INPUT | OUTPUT | ADC,
        2: INPUT | OUTPUT | ADC,
        3: INPUT | OUTPUT | ADC,
        4: INPUT | OUTPUT | ADC,
        5: INPUT | OUTPUT | ADC,
        6: INPUT | OUTPUT,
        7: INPUT | OUTPUT,
        8: INPUT | OUTPUT,
        9: INPUT | OUTPUT,
        10: INPUT | OUTPUT,
        # 11: INPUT | OUTPUT, # VDD_SPI (backup power line for in-package flash)
        # 12: INPUT | OUTPUT, # Used for flash
        # 13: INPUT | OUTPUT, # Used for flash
        # 14: INPUT | OUTPUT, # Used for flash
        # 15: INPUT | OUTPUT, # Used for flash
        # 16: INPUT | OUTPUT, # Used for flash
        # 17: INPUT | OUTPUT, # Used for flash
        18: INPUT | OUTPUT,
        19: INPUT | OUTPUT,
        20: INPUT | OUTPUT,
        21: INPUT | OUTPUT,
    }
}


def get_pin_function(mcu, pin):
    if mcu in mcu_pin_functions:
        if pin in mcu_pin_functions[mcu]:
            return mcu_pin_functions[mcu][pin]
    return None
