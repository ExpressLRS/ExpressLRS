Import("env")
import os
import hashlib

def parse_flags(path):
    build_flags = env['BUILD_FLAGS']
    try:
        with open(path, "r") as _f:
            for define in _f:
                define = define.strip()
                if define.startswith("-D"):
                    if "My_Binding_Phrase" in define:
                        bindingPhraseHash = hashlib.md5(define.encode()).digest()
                        define = "-DMY_UID=" + str(bindingPhraseHash[0]) + "," + str(bindingPhraseHash[1]) + "," + str(bindingPhraseHash[2]) + ","+ str(bindingPhraseHash[3]) + "," + str(bindingPhraseHash[4]) + "," + str(bindingPhraseHash[5])
                    build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

parse_flags("user_defines.txt")

print("build flags: %s" % env['BUILD_FLAGS'])
