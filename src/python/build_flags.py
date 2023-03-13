Import("env")
from genericpath import exists
import os
import sys
import hashlib
import fnmatch
import time
import re
import melodyparser
import elrs_helpers

build_flags = env.get('BUILD_FLAGS', [])
json_flags = {}
UIDbytes = ""
define = ""
target_name = env.get('PIOENV', '').upper()

isRX = True if '_RX_' in target_name else False

def print_error(error):
    time.sleep(1)
    sys.stdout.write("\n\n\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;31m!!!             ExpressLRS Warning Below             !!!\n")
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;30m  %s \n" % error)
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\n\n")
    sys.stdout.flush()
    time.sleep(3)
    raise Exception('!!! %s !!!' % error)


def dequote(str):
    if str[0] == '"' and str[-1] == '"':
        return str[1:-1]
    return str

def process_json_flag(define):
    parts = re.search("-D(.*)\s*=\s*(.*)$", define)
    if parts and define.startswith("-D"):
        if parts.group(1) == "MY_BINDING_PHRASE":
            json_flags['uid'] = [x for x in hashlib.md5(define.encode()).digest()[0:6]]
        if parts.group(1) == "HOME_WIFI_SSID":
            json_flags['wifi-ssid'] = dequote(parts.group(2))
        if parts.group(1) == "HOME_WIFI_PASSWORD":
            json_flags['wifi-password'] = dequote(parts.group(2))
        if parts.group(1) == "AUTO_WIFI_ON_INTERVAL":
            parts = re.search("-D(.*)\s*=\s*\"?([0-9]+).*\"?$", define)
            json_flags['wifi-on-interval'] = int(dequote(parts.group(2)))
        if parts.group(1) == "TLM_REPORT_INTERVAL_MS"  and not isRX:
            parts = re.search("-D(.*)\s*=\s*\"?([0-9]+).*\"?$", define)
            json_flags['tlm-interval'] = int(dequote(parts.group(2)))
        if parts.group(1) == "FAN_MIN_RUNTIME"  and not isRX:
            parts = re.search("-D(.*)\s*=\s*\"?([0-9]+).*\"?$", define)
            json_flags['fan-runtime'] = int(dequote(parts.group(2)))
        if parts.group(1) == "RCVR_UART_BAUD" and isRX:
            parts = re.search("-D(.*)\s*=\s*\"?([0-9]+).*\"?$", define)
            json_flags['rcvr-uart-baud'] = int(dequote(parts.group(2)))
        if parts.group(1) == "USE_AIRPORT_AT_BAUD":
            parts = re.search("-D(.*)\s*=\s*\"?([0-9]+).*\"?$", define)
            json_flags['is-airport'] = True
            if isRX:
                json_flags['rcvr-uart-baud'] = int(dequote(parts.group(2)))
            else:
                json_flags['airport-uart-baud'] = int(dequote(parts.group(2)))
    if define == "-DUART_INVERTED" and not isRX:
        json_flags['uart-inverted'] = True
    if define == "-DUNLOCK_HIGHER_POWER"  and not isRX:
        json_flags['unlock-higher-power'] = True
    if define == "-DLOCK_ON_FIRST_CONNECTION" and isRX:
        json_flags['lock-on-first-connection'] = True

def process_build_flag(define):
    if define.startswith("-D") or define.startswith("!-D"):
        if "MY_BINDING_PHRASE" in define:
            build_flags.append(define)
            bindingPhraseHash = hashlib.md5(define.encode()).digest()
            UIDbytes = ",".join(list(map(str, bindingPhraseHash))[0:6])
            define = "-DMY_UID=" + UIDbytes
            sys.stdout.write("\u001b[32mUID bytes: " + UIDbytes + "\n")
            sys.stdout.flush()
        if "MY_STARTUP_MELODY=" in define:
            parsedMelody = melodyparser.parse(define.split('"')[1::2][0])
            define = "-DMY_STARTUP_MELODY_ARR=\"" + parsedMelody + "\""
        if "HOME_WIFI_SSID=" in define:
            parts = re.search("(.*)=\w*\"(.*)\"$", define)
            if parts and parts.group(2):
                define = "-DHOME_WIFI_SSID=" + string_to_ascii(parts.group(2))
        if "HOME_WIFI_PASSWORD=" in define:
            parts = re.search("(.*)=\w*\"(.*)\"$", define)
            if parts and parts.group(2):
                define = "-DHOME_WIFI_PASSWORD=" + string_to_ascii(parts.group(2))
        if "DEVICE_NAME=" in define:
            parts = re.search("(.*)=\w*'?\"(.*)\"'?$", define)
            if parts and parts.group(2):
                env['DEVICE_NAME'] = parts.group(2)
        if not define in build_flags:
            build_flags.append(define)

def parse_flags(path):
    global build_flags
    global json_flags
    try:
        with open(path, "r") as _f:
            for define in _f:
                define = define.strip()
                process_build_flag(define)
                process_json_flag(define)

    except IOError:
        print("File '%s' does not exist" % path)

def process_flags(path):
    global build_flags
    if not os.path.isfile(path):
        return
    parse_flags(path)

def condense_flags():
    global build_flags
    for line in build_flags:
        # Some lines have multiple flags so this will split them and remove them all
        for flag in re.findall("!-D\s*[^\s]+", line):
            build_flags = [x.replace(flag,"") for x in build_flags] # remove the removal flag
            build_flags = [x.replace(flag[1:],"") for x in build_flags] # remove the flag if it matches the removal flag
    build_flags = [x for x in build_flags if (x.strip() != "")] # remove any blank items

def version_to_env():
    ver = elrs_helpers.get_git_version()
    env.Append(GIT_SHA = ver['sha'], GIT_VERSION= ver['version'])

def string_to_ascii(str):
    return ",".join(["%s" % ord(char) for char in str])

def get_git_sha():
    return string_to_ascii(env.get('GIT_SHA'))

def get_version():
    return string_to_ascii(env.get('GIT_VERSION'))

process_flags("user_defines.txt")
process_flags("super_defines.txt") # allow secret super_defines to override user_defines
version_to_env()
build_flags.append("-DLATEST_COMMIT=" + get_git_sha())
build_flags.append("-DLATEST_VERSION=" + get_version())
build_flags.append("-DTARGET_NAME=" + re.sub("_VIA_.*", "", target_name))
condense_flags()

if '-DRADIO_SX127X=1' in build_flags:
    # disallow setting 2400s for 900
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_ISM_2400') or \
        fnmatch.filter(build_flags, '*-DRegulatory_Domain_EU_CE_2400'):
        print_error('Regulatory_Domain 2400 not compatible with RADIO_SX127X')

    # require a domain be set for 900
    if not fnmatch.filter(build_flags, '*-DRegulatory_Domain*'):
        print_error('Please define a Regulatory_Domain in user_defines.txt')

    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_AU_915'):
        json_flags['domain'] = 0
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_FCC_915'):
        json_flags['domain'] = 1
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_EU_868'):
        json_flags['domain'] = 2
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_IN_866'):
        json_flags['domain'] = 3
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_AU_433'):
        json_flags['domain'] = 4
    if fnmatch.filter(build_flags, '*-DRegulatory_Domain_EU_433'):
        json_flags['domain'] = 5
else:
    json_flags['domain'] = 0

# Remove ISM_2400 domain flag if not unit test, it is defined per target config
if fnmatch.filter(build_flags, '*Regulatory_Domain_ISM_2400*') and \
        target_name != "NATIVE":
    build_flags = [f for f in build_flags if "Regulatory_Domain_ISM_2400" not in f]

env['OPTIONS_JSON'] = json_flags
env['BUILD_FLAGS'] = build_flags
sys.stdout.write("\nbuild flags: %s\n\n" % build_flags)

if fnmatch.filter(build_flags, '*PLATFORM_ESP32*'):
    sys.stdout.write("\u001b[32mBuilding for ESP32 Platform\n")
elif fnmatch.filter(build_flags, '*PLATFORM_STM32*'):
    sys.stdout.write("\u001b[32mBuilding for STM32 Platform\n")
elif fnmatch.filter(build_flags, '*PLATFORM_ESP8266*'):
    sys.stdout.write("\u001b[32mBuilding for ESP8266/ESP8285 Platform\n")
    if fnmatch.filter(build_flags, '-DAUTO_WIFI_ON_INTERVAL*'):
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_INTERVAL = ON\n")
    else:
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_INTERVAL = OFF\n")

sys.stdout.flush()
time.sleep(.5)

# Set upload_protovol = 'custom' for STM32 MCUs
#  otherwise firmware.bin is not generated
stm = env.get('PIOPLATFORM', '') in ['ststm32']
if stm:
    env['UPLOAD_PROTOCOL'] = 'custom'
