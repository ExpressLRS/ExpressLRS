Import("env")
import os
import sys
import subprocess
import hashlib
import fnmatch
import time
import re
import melodyparser

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
        x = parts.group(1) + '="' + parts.group(2).translate(str.maketrans({
            "!": "\\\\\\\\041",
            "\"": "\\\\\\\\042",
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
    build_flags = [x.replace("!", "") for x in build_flags]  # remove the !
    build_flags = [x for x in build_flags if (x.strip() != "")] # remove any blank items
    build_flags = [escapeChars(x) for x in build_flags] # perform escaping of flags with values

def get_git_sha():
    # Don't try to pull the git revision when doing tests, as
    # `pio remote test` doesn't copy the entire repository, just the files
    if env['PIOPLATFORM'] == "native":
        return "012345"

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
    return ",".join(["%s" % ord(x) for x in sha[:6]])

def get_git_version():
    # Don't try to pull the git revision when doing tests, as
    # `pio remote test` doesn't copy the entire repository, just the files
    if env['PIOPLATFORM'] == "native":
        return "001122334455"

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

    ver = "ver. unknown"
    if git:
        try:
            git_repo = git.Repo(
                os.path.abspath(os.path.join(os.getcwd(), os.pardir)),
                search_parent_directories=False)
            try:
                ver = re.sub(r".*/", "", git_repo.git.describe("--all", "--exact-match"))
            except git.exc.GitCommandError:
                try:
                    ver = git_repo.git.symbolic_ref("-q", "--short", "HEAD")
                except git.exc.GitCommandError:
                    ver = "ver. unknown"
            hash = git_repo.git.rev_parse("--short", "HEAD")
        except git.InvalidGitRepositoryError:
            pass
    return ",".join(["%s" % ord(char) for char in ver])

process_flags("user_defines.txt")
process_flags("super_defines.txt") # allow secret super_defines to override user_defines
build_flags.append("-DLATEST_COMMIT=" + get_git_sha())
build_flags.append("-DLATEST_VERSION=" + get_git_version())
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
