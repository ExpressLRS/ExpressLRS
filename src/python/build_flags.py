Import("env")
import os
import sys
import hashlib
import fnmatch
import time
import re
import melodyparser
import elrs_helpers

build_flags = env.get('BUILD_FLAGS', [])
UIDbytes = ""
define = ""
target_name = env.get('PIOENV', '').upper()

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


def parse_flags(path):
    global build_flags
    try:
        with open(path, "r") as _f:
            for define in _f:
                define = define.strip()
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
                    if not define in build_flags:
                        build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

def process_flags(path):
    global build_flags
    if not os.path.isfile(path):
        return
    parse_flags(path)

def escapeChars(x):
    parts = re.search("(.*)=\w*\"(.*)\"$", x)
    if parts and parts.group(2):
        if parts.group(1) == "-DMY_STARTUP_MELODY_ARR": # ignoring escape chars for startup melody
            return x
        x = parts.group(1) + '="' + parts.group(2).translate(str.maketrans({
            "!": "\\\\\\\\041",
            "\"": "\\\\\\\\042",
            "#": "\\\\\\\\043",
            "$": "\\\\\\\\044",
            "&": "\\\\\\\\046",
            "'": "\\\\\\\\047",
            "(": "\\\\\\\\050",
            ")": "\\\\\\\\051",
            ",": "\\\\\\\\054",
            ";": "\\\\\\\\073",
            "<": "\\\\\\\\074",
            ">": "\\\\\\\\076",
            "\\": "\\\\\\\\134",
            "`": "\\\\\\\\140",
            "|": "\\\\\\\\174"
        })) + '"'
    return x

def condense_flags():
    global build_flags
    for line in build_flags:
        # Some lines have multiple flags so this will split them and remove them all
        for flag in re.findall("!-D\s*[^\s]+", line):
            build_flags = [x.replace(flag[1:],"") for x in build_flags] # remove the flag which will just leave ! in their place
    build_flags = [escapeChars(x) for x in build_flags] # perform escaping of flags with values
    build_flags = [x.replace("!", "") for x in build_flags]  # remove the !
    build_flags = [x for x in build_flags if (x.strip() != "")] # remove any blank items

def version_to_env():
    ver = elrs_helpers.get_git_version()
    env.Append(GIT_SHA = ver['sha'], GIT_VERSION= ver['version'])

def regulatory_domain_to_env():
    regions = [("AU_915", "AU915"), ("EU_868", "EU868"), ("IN_866", "IN866"), ("AU_433", "AU433"), ("EU_433", "EU433"), ("FCC_915","FCC915"), ("ISM_2400", "ISM2G4"), ("EU_CE_2400", "CE2G4")]
    retVal = "UNK"
    if ("_2400" in target_name or \
        '-DRADIO_2400=1' in build_flags) and \
        '-DRegulatory_Domain_EU_CE_2400' not in build_flags:
        retVal = "ISM2G4"
    else:
        for k, v in regions:
            if fnmatch.filter(build_flags, '*-DRegulatory_Domain_'+k):
                retVal = v
                break
    env.Append(REG_DOMAIN = retVal)

def string_to_ascii(str):
    return ",".join(["%s" % ord(char) for char in str])

def get_git_sha():
    return string_to_ascii(env.get('GIT_SHA'))

def get_ver_and_reg():
    return string_to_ascii(env.get('GIT_VERSION') + " " + env.get('REG_DOMAIN'))


process_flags("user_defines.txt")
process_flags("super_defines.txt") # allow secret super_defines to override user_defines
version_to_env()
regulatory_domain_to_env()
build_flags.append("-DLATEST_COMMIT=" + get_git_sha())
build_flags.append("-DLATEST_VERSION=" + get_ver_and_reg()) # version and domain
build_flags.append("-DTARGET_NAME=" + re.sub("_VIA_.*", "", target_name))
condense_flags()

if "_2400" not in target_name and \
        not fnmatch.filter(build_flags, '-DRADIO_2400=1') and \
        not fnmatch.filter(build_flags, '*-DRegulatory_Domain*'):
    print_error('Please define a Regulatory_Domain in user_defines.txt')

if fnmatch.filter(build_flags, '*Regulatory_Domain_ISM_2400*') and \
        target_name != "NATIVE":
    # Remove ISM_2400 domain flag if not unit test, it is defined per target config
    build_flags = [f for f in build_flags if "Regulatory_Domain_ISM_2400" not in f]

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
