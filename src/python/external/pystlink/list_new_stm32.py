import urllib.request
import json


def fix_cpu_type(cpu_type):
    cpu_type = cpu_type.upper()
    # now support only STM32
    if cpu_type.startswith('STM32'):
        # change character on 10 position to 'x' where is package size code
        if len(cpu_type) > 9:
            cpu_type = list(cpu_type)
            cpu_type[9] = 'x'
            cpu_type = ''.join(cpu_type)
        return cpu_type
    raise Exception('"%s" is not STM32 family' % cpu_type)

print("Downloading list of all STM32 MCUs from ST.com...")
# req = urllib.request.urlopen('http://www.st.com/content/st_com/en/products/microcontrollers/stm32-32-bit-arm-cortex-mcus.product-grid.html/SC1169.json')

URLS = [
    'http://www.st.com/content/st_com/en/products/microcontrollers/stm32-32-bit-arm-cortex-mcus.product-grid.html/SC2154.json',
    'http://www.st.com/content/st_com/en/products/microcontrollers/stm32-32-bit-arm-cortex-mcus.product-grid.html/SC2155.json',
    'http://www.st.com/content/st_com/en/products/microcontrollers/stm32-32-bit-arm-cortex-mcus.product-grid.html/SC2157.json',
]

def download_data(url):
    req = urllib.request.urlopen(url)
    res = req.read()
    print("downloaded: %d KBytes" % (len(res) / 1024))
    return json.loads(res.decode('utf-8'))

# columns has important keys:
#  - id
#  - name
# raw_json['columns']
REQUESTED_COLUMNS = {
    'Part Number': 'type',
    'Core': 'core',
    'Operating Frequency': 'freq',
    'FLASH Size': 'flash_size',
    'Data E2PROM': 'eeprom_size',
    'RAM Size': 'sram_size',
}

raw_jsons = [download_data(url) for url in URLS]

columns = {}
for raw_json in raw_jsons:
    for column in raw_json['columns']:
        if column['name'] in REQUESTED_COLUMNS:
            columns[column['id']] = REQUESTED_COLUMNS[column['name']]

mcus = []
for raw_json in raw_jsons:
    for row in raw_json['rows']:
        mcu = {}
        mcu['url'] = 'http://www.st.com' + row['productFolderUrl']
        for cell in row['cells']:
            column_id = cell['columnId']
            if column_id in columns:
                column_name = columns[column_id]
                mcu[column_name] = cell['value']
        mcus.append(mcu)
print("MCUs on ST.com is: %d" % len(mcus))

supported_mcus = {}
for devs in pystlink.lib.stm32devices.DEVICES:
    for dev in devs['devices']:
        for d in dev['devices']:
            supported_mcus[d['type']] = d

unsupported_mcus = {}
wrong_param_mcus = {}

for mcu in mcus:
    mcu_type_fixed = fix_cpu_type(mcu['type'])
    mcu_type = mcu.get('type')
    core = mcu.get('core')
    freq = mcu.get('freq')
    if freq is None:
        mcu['freq'] = ''
        freq = ''
    if freq.isnumeric():
        freq = int(freq)
    flash_size = mcu.get('flash_size')
    if flash_size is None:
        mcu['flash_size'] = ''
        flash_size = ''
    if flash_size.isnumeric():
        flash_size = int(flash_size)
    sram_size = mcu.get('sram_size')
    if sram_size is None:
        mcu['sram_size'] = ''
        sram_size = ''
    if sram_size.isnumeric():
        sram_size = int(sram_size)
    eeprom_size = mcu.get('eeprom_size')
    eeprom_size = int(eeprom_size) / 1024 if eeprom_size else 0
    if eeprom_size % 1 == 0:
        eeprom_size = int(eeprom_size)
    mcu['eeprom_size'] = eeprom_size
    if mcu_type_fixed in supported_mcus:
        supported_mcu = supported_mcus[mcu_type_fixed]
        ok = True
        if supported_mcu['freq'] != freq:
            ok = False
        if supported_mcu['flash_size'] != flash_size:
            ok = False
        if supported_mcu['sram_size'] != sram_size:
            ok = False
        if supported_mcu['eeprom_size'] != eeprom_size:
            ok = False
        if not ok:
            mcu['supported_mcu'] = supported_mcu
            mcu['type_fixed'] = mcu_type_fixed
            wrong_param_mcus[mcu_type] = mcu
    else:
        mcu['type_fixed'] = mcu_type_fixed
        unsupported_mcus[mcu_type] = mcu

print("%-15s %-15s %6s %6s %6s %6s" % ('type', 'core', 'flash', 'sram', 'eeprom', 'freq'))
if unsupported_mcus:
    print('---- unsupported mcus (%d) ----' % len(unsupported_mcus))
    for k in sorted(unsupported_mcus.keys()):
        v = unsupported_mcus[k]
        print(v['url'])
        print("%-15s %-15s %6s %6s %6s %6s" % (
            k, v.get('core', ''), v['flash_size'], v['sram_size'], v.get('eeprom_size'), v['freq']
        ))
if wrong_param_mcus:
    print('---- mcus with wrong params (%d) ----' % len(wrong_param_mcus))
    for k in sorted(wrong_param_mcus.keys()):
        v = wrong_param_mcus[k]
        s = v['supported_mcu']
        print(v['url'])
        print("%-15s %-15s %6s %6s %6s %6s" % (
            k, v.get('core', ''), v['flash_size'], v['sram_size'], v.get('eeprom_size'), v['freq']
        ))
        print("%-15s %-15s %6s %6s %6s %6s" % (
            '(pystlink)', v.get('core', ''), s['flash_size'], s['sram_size'], s.get('eeprom_size'), s['freq']
        ))
