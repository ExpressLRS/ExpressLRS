Import("env")
import os
import sys
import subprocess
import hashlib
import fnmatch
import time
import re


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
                        build_flags.append(define)
                        bindingPhraseHash = hashlib.md5(define.encode()).digest()
                        UIDbytes = ",".join(list(map(str, bindingPhraseHash))[0:6])
                        define = "-DMY_UID=" + UIDbytes
                        sys.stdout.write("\u001b[32mUID bytes: " + UIDbytes + "\n")
                        sys.stdout.flush()
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
                ver = git_repo.git.describe("--tags", "--exact-match")
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
build_flags.append("-DTARGET_NAME=" + re.sub("_VIA_.*", "", env['PIOENV'].upper()))
condense_flags()

env['BUILD_FLAGS'] = build_flags
print("build flags: %s" % env['BUILD_FLAGS'])

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

time.sleep(1)
