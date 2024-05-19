from enum import Enum
from pin_functions import get_pin_function


class FieldType(Enum):
    INT = 1
    BOOL = 2
    FLOAT = 3
    ARRAY = 4
    PIN = 100   # marker
    INPUT = 101
    OUTPUT = 102
    BIDIR = 103
    ADC = 104


hardware_fields = {
    "serial_rx": FieldType.INPUT,
    "serial_tx": FieldType.OUTPUT,
    "serial1_rx": FieldType.INPUT,
    "serial1_tx": FieldType.OUTPUT,
    "radio_busy": FieldType.INPUT,
    "radio_busy_2": FieldType.INPUT,
    "radio_dio0": FieldType.INPUT,
    "radio_dio0_2": FieldType.INPUT,
    "radio_dio1": FieldType.INPUT,
    "radio_dio1_2": FieldType.INPUT,
    "radio_dio2": FieldType.INPUT,
    "radio_miso": FieldType.INPUT,
    "radio_mosi": FieldType.OUTPUT,
    "radio_nss": FieldType.OUTPUT,
    "radio_nss_2": FieldType.OUTPUT,
    "radio_rst": FieldType.OUTPUT,
    "radio_rst_2": FieldType.OUTPUT,
    "radio_sck": FieldType.OUTPUT,
    "radio_dcdc": FieldType.BOOL,
    "radio_rfo_hf": FieldType.BOOL,
    "ant_ctrl": FieldType.OUTPUT,
    "ant_ctrl_compl": FieldType.OUTPUT,
    "power_enable": FieldType.OUTPUT,
    "power_apc1": FieldType.OUTPUT,
    "power_apc2": FieldType.OUTPUT,
    "power_rxen": FieldType.OUTPUT,
    "power_txen": FieldType.OUTPUT,
    "power_rxen_2": FieldType.OUTPUT,
    "power_txen_2": FieldType.OUTPUT,
    "power_lna_gain": FieldType.INT,
    "power_min": FieldType.INT,
    "power_high": FieldType.INT,
    "power_max": FieldType.INT,
    "power_default": FieldType.INT,
    "power_pdet": FieldType.ADC,
    "power_pdet_intercept": FieldType.FLOAT,
    "power_pdet_slope": FieldType.FLOAT,
    "power_control": FieldType.INT,
    "power_values": FieldType.ARRAY,
    "power_values2": FieldType.ARRAY,
    "power_values_dual": FieldType.ARRAY,
    "joystick": FieldType.ADC,
    "joystick_values": FieldType.ARRAY,
    "five_way1": FieldType.INPUT,
    "five_way2": FieldType.INPUT,
    "five_way3": FieldType.INPUT,
    "button": FieldType.INPUT,
    "button_led_index": FieldType.INT,
    "button2": FieldType.INPUT,
    "button2_led_index": FieldType.INT,
    "led": FieldType.OUTPUT,
    "led_blue": FieldType.OUTPUT,
    "led_blue_invert": FieldType.BOOL,
    "led_green": FieldType.OUTPUT,
    "led_green_invert": FieldType.BOOL,
    "led_green_red": FieldType.OUTPUT,
    "led_red": FieldType.OUTPUT,
    "led_red_invert": FieldType.BOOL,
    "led_red_green": FieldType.OUTPUT,
    "led_rgb": FieldType.OUTPUT,
    "led_rgb_isgrb": FieldType.BOOL,
    "ledidx_rgb_status": FieldType.ARRAY,
    "ledidx_rgb_vtx": FieldType.ARRAY,
    "ledidx_rgb_boot": FieldType.ARRAY,
    "screen_cs": FieldType.OUTPUT,
    "screen_dc": FieldType.OUTPUT,
    "screen_mosi": FieldType.OUTPUT,
    "screen_rst": FieldType.OUTPUT,
    "screen_sck": FieldType.OUTPUT,
    "screen_sda": FieldType.OUTPUT,
    "screen_type": FieldType.INT,
    "screen_reversed": FieldType.BOOL,
    "screen_bl": FieldType.OUTPUT,
    "use_backpack": FieldType.BOOL,
    "debug_backpack_baud": FieldType.INT,
    "debug_backpack_rx": FieldType.INPUT,
    "debug_backpack_tx": FieldType.OUTPUT,
    "backpack_boot": FieldType.OUTPUT,
    "backpack_en": FieldType.OUTPUT,
    "passthrough_baud": FieldType.INT,
    "i2c_scl": FieldType.OUTPUT,
    "i2c_sda": FieldType.BIDIR,
    "misc_gsensor_int": FieldType.INPUT,
    "misc_buzzer": FieldType.OUTPUT,
    "misc_fan_en": FieldType.OUTPUT,
    "misc_fan_pwm": FieldType.OUTPUT,
    "misc_fan_tacho": FieldType.INPUT,
    "misc_fan_speeds": FieldType.ARRAY,
    "gsensor_stk8xxx": FieldType.BOOL,
    "thermal_lm75a": FieldType.BOOL,
    "pwm_outputs": FieldType.ARRAY,
    "vbat": FieldType.ADC,
    "vbat_offset": FieldType.INT,
    "vbat_scale": FieldType.INT,
    "vbat_atten": FieldType.INT,
    "vtx_amp_pwm": FieldType.OUTPUT,
    "vtx_amp_vpd": FieldType.ADC,
    "vtx_amp_vref": FieldType.OUTPUT,
    "vtx_nss": FieldType.OUTPUT,
    "vtx_miso": FieldType.INPUT,
    "vtx_mosi": FieldType.OUTPUT,
    "vtx_sck": FieldType.OUTPUT,
    "vtx_amp_vpd_25mW": FieldType.ARRAY,
    "vtx_amp_vpd_100mW": FieldType.ARRAY,
    "vtx_amp_pwm_25mW": FieldType.ARRAY,
    "vtx_amp_pwm_100mW": FieldType.ARRAY,
    "ir_transponder": FieldType.OUTPUT,
    "gyro_nss": FieldType.OUTPUT,
    "gyro_miso": FieldType.INPUT,
    "gyro_mosi": FieldType.OUTPUT,
    "gyro_sck": FieldType.OUTPUT,
    "gyro_int": FieldType.INPUT,
    "adc_a1": FieldType.ADC,
    "adc_a2": FieldType.ADC
}

field_groups = {
    "all": [
        # if one of the first group then all the first and second groups and
        # at-least one of the third group must also be defined
        [["serial_rx", "serial_tx"], [], []],
        [["serial1_rx", "serial1_tx"], [], []],
        [["power_min", "power_high", "power_max", "power_default", "power_control", "power_values"], [], []],
        [["debug_backpack_baud", "debug_backpack_rx", "debug_backpack_tx"], [], []],
        [["use_backpack"], ["debug_backpack_baud", "debug_backpack_rx", "debug_backpack_tx"], []],
        [["backpack_boot", "backpack_en"], ["use_backpack", "debug_backpack_baud", "debug_backpack_rx", "debug_backpack_tx"], []],
        [["i2c_scl", "i2c_sda"], [], []],
        [["joystick", "joystick_values"], [], []],
        [["five_way1", "five_way2", "five_way3"], [], []],
        [["misc_fan_pwm", "misc_fan_speeds"], [], []],
        [["vbat", "vbat_offset", "vbat_scale"], [], []],
        [["power_pdet", "power_pdet_intercept", "power_pdet_slope"], [], []],
        [["screen_sda"], ["screen_sck", "screen_type"], []],
        [["screen_cs", "screen_dc", "screen_mosi"], ["screen_type", "screen_sck", "screen_rst"], []],
        [["vtx_amp_pwm", "vtx_amp_vpd", "vtx_amp_vref", "vtx_nss", "vtx_miso", "vtx_mosi", "vtx_sck", "vtx_amp_vpd_25mW", "vtx_amp_vpd_100mW"], [], []],
        [["gyro_nss", "gyro_miso", "gyro_mosi", "gyro_sck", "gyro_int"], [], []]
    ],
    "2400": [
        [["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], [], ["radio_rst", "pwm_outputs"]],
        [["radio_busy"], ["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], ["radio_rst", "pwm_outputs"]],
        [["radio_rst"], ["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], []],
        [["radio_dio1_2", "radio_nss_2"], ["radio_miso", "radio_mosi", "radio_sck"], ["radio_rst_2", "radio_rst", "pwm_outputs"]],
        [["radio_busy_2"], ["radio_dio1_2", "radio_miso", "radio_mosi", "radio_sck", "radio_nss_2"], ["radio_rst_2", "radio_rst", "pwm_outputs"]],
        [["radio_rst_2"], ["radio_dio1_2", "radio_miso", "radio_mosi", "radio_sck", "radio_nss_2"], []]
    ],
    "900": [
        [["radio_dio0", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], [], ["radio_rst", "pwm_outputs"]],
        [["radio_rst"], ["radio_dio0", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], []],
        [["radio_dio0_2", "radio_nss_2"], ["radio_miso", "radio_mosi", "radio_sck"], ["radio_rst_2", "radio_rst", "pwm_outputs"]],
        [["radio_rst_2"], ["radio_dio0_2", "radio_miso", "radio_mosi", "radio_sck", "radio_nss_2"], []]
    ],
    "dual": [
        [["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], [], ["radio_rst", "pwm_outputs"]],
        [["radio_busy"], ["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], ["radio_rst", "pwm_outputs"]],
        [["radio_rst"], ["radio_dio1", "radio_miso", "radio_mosi", "radio_sck", "radio_nss"], []],
        [["radio_dio1_2", "radio_nss_2"], ["radio_miso", "radio_mosi", "radio_sck"], ["radio_rst_2", "radio_rst", "pwm_outputs"]],
        [["radio_busy_2"], ["radio_dio1_2", "radio_miso", "radio_mosi", "radio_sck", "radio_nss_2"], ["radio_rst_2", "radio_rst", "pwm_outputs"]],
        [["radio_rst_2"], ["radio_dio1_2", "radio_miso", "radio_mosi", "radio_sck", "radio_nss_2"], []]
    ]
}

mutually_exclusive_groups = [
    ["ant_ctrl", "ant_ctrl_compl"]
]

allowable_duplicates = [
    ['serial_rx', 'serial_tx'],
    ['screen_sck', 'i2c_scl'],
    ['screen_sda', 'screen_mosi', 'i2c_sda']
]
allowable_pwm_shared = [
    'serial_rx', 'serial_tx', 'serial1_rx', 'serial1_tx', 'i2c_scl', 'i2c_sda'
]

used_pins = {}


def ignore_undef_pins(pair):
    key, value = pair
    # ignore keys that start with a comment character
    if key[0] in '/#;':
        return False

    # FIXME, <0 should really be -1, but I used -pin number to mean inverted for the backlight
    if key in hardware_fields and hardware_fields[key].value > FieldType.PIN.value and value < 0:
        return False
    else:
        return True


def validate(target, layout, device, type):
    global used_pins
    had_error = False
    used_pins = {}
    for field in dict(filter(ignore_undef_pins, layout.items())):
        # Ensure that the layout field is a valid field from the hardware list
        if field not in hardware_fields.keys():
            print(f'device "{target}" has an unknown field name {field}')
            had_error = True
        else:
            had_error |= validate_pin_uniqueness(target, layout, field)
            had_error |= validate_grouping(target, layout, field, type)
            had_error |= validate_pin_function(target, layout, field, device['platform'])
    had_error |= validate_power_config(target, layout)
    had_error |= validate_backpack(target, layout)
    had_error |= validate_joystick(target, layout)
    had_error |= validate_pwm_outputs(target, layout)
    had_error |= validate_vtx_amp_pwm(target, layout, device['platform'])
    return had_error


def validate_grouping(target, layout, field, radio):
    had_error = validate_field_grouping(target, layout, field, 'all')
    if radio.endswith('_2400'):
        had_error |= validate_field_grouping(target, layout, field, '2400')
    elif radio.endswith('_900'):
        had_error |= validate_field_grouping(target, layout, field, '900')
    elif radio.endswith('_dual'):
        had_error |= validate_field_grouping(target, layout, field, 'dual')
    return had_error


def validate_field_grouping(target, layout, field, field_group):
    had_error = False
    for group in field_groups[field_group]:
        if field in group[0]:
            for must in group[0] + group[1]:
                if must not in layout:
                    print(f'device "{target}" because "{field}" is defined all other related fields must also be defined {must}')
                    print(f'\t{group[0] + group[1]}')
                    had_error = True
            found = True if group[2] == [] else False
            for one in group[2]:
                if one in layout:
                    found = True
            if not found:
                print(f'device "{target}" because "{field}" is defined at least one of the following fields must also be {group[2]}')
                had_error = True
    return had_error


def validate_pin_uniqueness(target, layout, field):
    had_error = False
    if hardware_fields[field].value > FieldType.PIN.value:
        pin = layout[field]
        if pin in used_pins.keys():
            allowed = False
            for duplicate in allowable_duplicates:
                if field in duplicate and used_pins[pin] in duplicate:
                    allowed = True
            if not allowed:
                print(f'device "{target}" PIN {pin} "{field}" is already assigned to "{used_pins[pin]}"')
                had_error = True
        else:
            used_pins[pin] = field
    return had_error


def validate_power_config(target, layout):
    had_error = False
    if 'power_values' in layout:
        power_values = layout['power_values']
        power_max = layout['power_max']
        power_min = layout['power_min']
        power_default = layout['power_default']
        power_high = layout['power_high']
        if power_min > power_max:
            print(f'device "{target}" power_min must be less than or equal to power_max')
            had_error = True
        if power_default < power_min or power_default > power_max:
            print(f'device "{target}" power_default must lie between power_min and power_max')
            had_error = True
        if power_high < power_min or power_high > power_max:
            print(f'device "{target}" power_high must lie between power_min and power_max')
            had_error = True
        if power_max - power_min + 1 != len(power_values):
            print(f'device "{target}" power_values must have the correct number of entries to match all values from power_min to power_max')
            had_error = power_max - power_min + 1 > len(power_values)
        if layout['power_control'] == 3 and 'power_apc2' not in layout:
            print(f'device "{target}" defines power_control as DACWRITE and power_apc2 is undefined')
            had_error = True
        if 'power_values2' in layout:
            if len(layout['power_values2']) != len(power_values):
                print(f'device "{target}" power_values2 must have the same number of entries as power_values')
                had_error = True
            if layout['power_control'] != 3:
                print(f'device "{target}" power_values2 is defined so power_control must be set to 3 (DACWRITE)')
                had_error = True
            if 'power_apc2' not in layout:
                print(f'device "{target}" power_values2 is defined so the power_apc2 pin must also be defined')
                had_error = True
    return had_error


def validate_backpack(target, layout):
    had_error = False
    if 'passthrough_baud' in layout:
        if layout['serial_rx'] == layout['serial_tx'] and layout['passthrough_baud'] != 230400:
            print(f'device "{target}" an external module with a backpack should set the baud rate to 230400')
            had_error = True
        if layout['serial_rx'] != layout['serial_tx'] and layout['passthrough_baud'] != 460800:
            print(f'device "{target}" an internal module with a backpack should set the baud rate to 460800')
            had_error = True
    return had_error


def validate_joystick(target, layout):
    had_error = False
    if 'joystick' in layout or 'joystick_values' in layout:
        if 'joystick' not in layout:
            print(f'device "{target}" joystick_values is defined so the joystick pin must also be defined')
            had_error = True
        elif 'joystick_values' not in layout:
            print(f'device "{target}" joystick is defined so the joystick_values must also be defined')
            had_error = True
        elif len(layout['joystick_values']) != 6:
            print(f'device "{target}" joystick_values must have 6 values defined')
            had_error = True
        else:
            for value in layout['joystick_values']:
                if value < 0 or value > 4095:
                    print(f'device "{target}" joystick_values must be between 0 and 4095 inclusive')
                    had_error = True
    return had_error


def validate_pwm_outputs(target, layout):
    had_error = False
    if 'pwm_outputs' in layout:
        for field in hardware_fields:
            if hardware_fields[field].value > FieldType.PIN.value and \
                    field in layout and \
                    layout[field] in layout['pwm_outputs'] and \
                    field not in allowable_pwm_shared:
                print(f'device "{target}" pwm_output pin {layout[field]} is not allowed to be shared with {field}')
                had_error = True
    return had_error


def validate_pin_function(target, layout, field, platform):
    if hardware_fields[field].value > FieldType.PIN.value:
        function = get_pin_function(platform, layout[field])
        if function is None:
            print(f'device "{target}" has an invalid pin number for {field}, {layout[field]}')
            return True
        if hardware_fields[field] == FieldType.INPUT and not (function & 1):
            print(f'device "{target}" pin for {field} must be assigned to a pin that supports INPUT')
            return True
    return False


def validate_vtx_amp_pwm(target, layout, platform):
    if platform == 'esp32' and 'vtx_amp_pwm' in layout:
        function = get_pin_function(platform, layout['vtx_amp_pwm'])
        if function is not None and function & 8 != 8:
            print(f'device "{target}" "vtx_amp_pwm" is preferred to be a DAC pin if possible, but PWM output is supported')
    return False
