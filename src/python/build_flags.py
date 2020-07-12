Import("env")
import os
import hashlib
import fnmatch
import time

def parse_flags(path):
    build_flags = env['BUILD_FLAGS']
    try:
        with open(path, "r") as _f:
            for define in _f:
                define = define.strip()
                if define.startswith("-D"):
                    if "MY_BINDING_PHRASE" in define:
                        bindingPhraseHash = hashlib.md5(define.encode()).digest()
                        define = "-DMY_UID=" + str(bindingPhraseHash[0]) + "," + str(bindingPhraseHash[1]) + "," + str(bindingPhraseHash[2]) + ","+ str(bindingPhraseHash[3]) + "," + str(bindingPhraseHash[4]) + "," + str(bindingPhraseHash[5])
                    build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

parse_flags("user_defines.txt")

print("build flags: %s" % env['BUILD_FLAGS'])

if not fnmatch.filter(env['BUILD_FLAGS'], '-DRegulatory_Domain*'):
    print("\033[47;31m %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% \n")
    print("\033[47;31m !!! ExpressLRS Warning Below !!! \n")
    print("\033[47;31m %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% \n")
    print("\033[47;30m Please uncomment a Regulatory_Domain in user_defines.txt \n")
    print("\033[47;31m %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
    print("\033[30m")
    time.sleep(3)
    raise Exception("!!! ExpressLRS Warning !!!")
