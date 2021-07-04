Import("env")
import os
import sys
import subprocess
import hashlib
import fnmatch
import time
import re
import melodyparser


build_flags = env['BUILD_FLAGS']
UIDbytes = ""
define = ""

def print_error(error):
    time.sleep(1)
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;31m!!!             ExpressLRS Warning Below             !!!\n")
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;30m  %s \n" % error)
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
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

def condense_flags():
    global build_flags
    for line in build_flags:
        # Some lines have multiple flags so this will split them and remove them all
        for flag in re.findall("!-D\s*[^\s]+", line):
            build_flags = [x.replace(flag[1:],"") for x in build_flags] # remove the flag which will just leave ! in their place
    build_flags = [x.replace("!", "") for x in build_flags]  # remove the !
    build_flags = [x for x in build_flags if (x.strip() != "")] # remove any blank items

def get_git_sha():
    # Don't try to pull the git revision when doing tests, as
    # `pio remote test` doesn't copy the entire repository, just the files
    if env['PIOPLATFORM'] == "native":
        return "0x00,0x11,0x22,0x33,0x44,0x55"

    try:
        import git
    except ImportError:
        sys.stdout.write("Installing GitPython")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "GitPython"])
        try:
            import git
        except ImportError:
            env.Execute("$PYTHONEXE -m pip install GitPython")
            try:
                import git
            except ImportError:
                git = None

    sha = None
    if git:
        try:
            git_repo = git.Repo(
                os.path.abspath(os.path.join(os.getcwd(), os.pardir)),
                search_parent_directories=False)
            git_root = git_repo.git.rev_parse("--show-toplevel")
            ExLRS_Repo = git.Repo(git_root)
            sha = ExLRS_Repo.head.object.hexsha
        except git.InvalidGitRepositoryError:
            pass
    if not sha:
        if os.path.exists("VERSION"):
            with open("VERSION") as _f:
                data = _f.readline()
                _f.close()
            sha = data.split()[1].strip()
        else:
            sha = "000000"
    return ",".join(["0x%s" % x for x in sha[:6]])

process_flags("user_defines.txt")
process_flags("super_defines.txt") # allow secret super_defines to override user_defines
build_flags.append("-DLATEST_COMMIT=" + get_git_sha())
build_flags.append("-DTARGET_NAME=" + re.sub("_VIA_.*", "", env['PIOENV'].upper()))
condense_flags()

env['BUILD_FLAGS'] = build_flags
print("build flags: %s" % env['BUILD_FLAGS'])

if not fnmatch.filter(env['BUILD_FLAGS'], '*-DRegulatory_Domain*'):
    print_error('Please define a Regulatory_Domain in user_defines.txt')

if fnmatch.filter(env['BUILD_FLAGS'], '*-DENABLE_TELEMETRY*') and not fnmatch.filter(env['BUILD_FLAGS'], '*-DHYBRID_SWITCHES_8*'):
    print_error('Telemetry requires HYBRID_SWITCHES_8')

if fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_ESP32*'):
    sys.stdout.write("\u001b[32mBuilding for ESP32 Platform\n")
elif fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_STM32*'):
    sys.stdout.write("\u001b[32mBuilding for STM32 Platform\n")
elif fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_ESP8266*'):
    sys.stdout.write("\u001b[32mBuilding for ESP8266/ESP8285 Platform\n")
    if fnmatch.filter(env['BUILD_FLAGS'], '-DAUTO_WIFI_ON_INTERVAL*'):
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_INTERVAL = ON\n")
    else:
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_INTERVAL = OFF\n")

if fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_AU_915*'):
    sys.stdout.write("\u001b[32mBuilding for SX1276 915AU\n")

elif fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_EU_868*'):
    sys.stdout.write("\u001b[32mBuilding for SX1276 868EU\n")

elif fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_AU_433*'):
    sys.stdout.write("\u001b[32mBuilding for SX1278 433AU\n")

elif fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_EU_433*'):
    sys.stdout.write("\u001b[32mBuilding for SX1278 433AU\n")

elif fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_FCC_915*'):
    sys.stdout.write("\u001b[32mBuilding for SX1276 915FCC\n")

elif fnmatch.filter(env['BUILD_FLAGS'], '*Regulatory_Domain_ISM_2400*'):
    sys.stdout.write("\u001b[32mBuilding for SX1280 2400ISM\n")

time.sleep(1)

# Set upload_protovol = 'custom' for STM32 MCUs
#  otherwise firmware.bin is not generated
stm = env.get('PIOPLATFORM', '') in ['ststm32']
if stm:
    env['UPLOAD_PROTOCOL'] = 'custom'

