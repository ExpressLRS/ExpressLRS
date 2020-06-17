Import("env")
import os
import fhss_random
import hashlib

def parse_flags(path):
    build_flags = env['BUILD_FLAGS']
    try:
        with open(path, "r") as _f:
            for line in _f:
                define = line.strip()
                if define.startswith("-D"):
                    if "MY_UID" in define and len(define.split(",")) != 6:
                        raise Exception("UID must be 6 bytes long")
                    elif "MY_PHRASE" in define:
                        define = define.split("=")[1]
                        define = define.replace('"', '').replace("'", "")
                        key = define.replace("-D", "")
                        if len(define) < 8:
                            raise Exception("MY_PHRASE must be at least 8 characters long")
                        md5 = hashlib.md5(key.encode()).digest()
                        define = "-DMY_UID=" + ",".join(["0x%02X"%r for r in md5[:6]])
                    build_flags.append(define)
    except IOError:
        print("File '%s' does not exist" % path)

parse_flags("user_defines.txt")

fhss_random.check_env_and_parse(env['BUILD_FLAGS'])

print("build flags: %s" % env['BUILD_FLAGS'])
