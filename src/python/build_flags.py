Import("env")
import os
import sys
import subprocess
import hashlib
import fnmatch
import time

def install(package):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])

try:
    from git import Repo
except ImportError:
    sys.stdout.write("Installing GitPython")
    install("GitPython")
    from git import Repo

build_flags = env['BUILD_FLAGS']

try:
    from git import Repo
except ImportError:
    env.Execute("$PYTHONEXE -m pip install GitPython")
    from git import Repo

build_flags = env['BUILD_FLAGS']

UIDbytes = ""
define = ""


def parse_flags(path):
    try:
        with open(path, "r") as _f:
            for define in _f:
                define = define.strip()
                if define.startswith("-D"):
                    if "MY_BINDING_PHRASE" in define:
                        if define == "-DMY_BINDING_PHRASE=\"default ExpressLRS binding phrase\"":
                            time.sleep(1)
                            sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
                            sys.stdout.write("\033[47;31m!!!                        ExpressLRS Warning Below                        !!!\n")
                            sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
                            sys.stdout.write("\033[47;30m                                                                              \n")
                            sys.stdout.write("\033[47;30m         Please change the default MY_BINDING_PHRASE in user_defines.txt      \n")
                            sys.stdout.write("\033[47;30m                                                                              \n")
                            sys.stdout.write("\033[47;30m  https://github.com/AlessandroAU/ExpressLRS/wiki/User-Defines#binding-phrase \n")
                            sys.stdout.write("\033[47;30m                                                                              \n")
                            sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
                            sys.stdout.flush()
                            time.sleep(3)
                            raise Exception('!!!  Please change the default MY_BINDING_PHRASE in user_defines.txt !!!')
                        bindingPhraseHash = hashlib.md5(define.encode()).digest()
                        UIDbytes = (str(bindingPhraseHash[0]) + "," + str(bindingPhraseHash[1]) + "," + str(bindingPhraseHash[2]) + ","+ str(bindingPhraseHash[3]) + "," + str(bindingPhraseHash[4]) + "," + str(bindingPhraseHash[5]))
                        define = "-DMY_UID=" + UIDbytes
                        sys.stdout.write("\u001b[32mUID bytes: " + UIDbytes + "\n")
                        sys.stdout.flush()
                    build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

parse_flags("user_defines.txt")

git_repo = Repo(os.getcwd(), search_parent_directories=True)
git_root = git_repo.git.rev_parse("--show-toplevel")
ExLRS_Repo = Repo(git_root)
sha = ExLRS_Repo.head.object.hexsha
build_flags.append("-DLATEST_COMMIT=0x"+sha[0]+",0x"+sha[1]+",0x"+sha[2]+",0x"+sha[3]+",0x"+sha[4]+",0x"+sha[5])

print("build flags: %s" % env['BUILD_FLAGS'])

if not fnmatch.filter(env['BUILD_FLAGS'], '-DRegulatory_Domain*'):
    time.sleep(1)
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;31m!!!             ExpressLRS Warning Below             !!!\n")
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.write("\033[47;30m  Please define a Regulatory_Domain in user_defines.txt \n")
    sys.stdout.write("\033[47;31m%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n")
    sys.stdout.flush()
    time.sleep(3)
    raise Exception('!!!  Please define a Regulatory_Domain in user_defines.txt !!!')

if fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_ESP32*'):
    sys.stdout.write("\u001b[32mBuilding for ESP32 Platform\n")
elif fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_STM32*'):
    sys.stdout.write("\u001b[32mBuilding for STM32 Platform\n")
elif fnmatch.filter(env['BUILD_FLAGS'], '*PLATFORM_ESP8266*'):
    sys.stdout.write("\u001b[32mBuilding for ESP8266/ESP8285 Platform\n")
    if fnmatch.filter(env['BUILD_FLAGS'], '-DAUTO_WIFI_ON_BOOT*'):
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_BOOT = ON\n")
    else:
        sys.stdout.write("\u001b[32mAUTO_WIFI_ON_BOOT = OFF\n")
        
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

time.sleep(1)
