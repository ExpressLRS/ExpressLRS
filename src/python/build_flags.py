Import("env")
import os

def parse_flags(path):
    build_flags = env['BUILD_FLAGS']
    try:
        with open(path, "r") as _f:
            for line in _f:
                defines = " ".join(line.split())
                defines = defines.replace(" = ", "=").split()
                for define in defines:
                    define = define.strip()
                    if define.startswith("-D"):
                        if "MY_UID" in define and len(define.split(",")) != 6:
                            raise Exception("UID must be 6 bytes long")
                        build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

parse_flags("user_defines.txt")

print("build flags: %s" % env['BUILD_FLAGS'])
