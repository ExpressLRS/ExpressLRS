Import("env")
import os

try:
    from git import Repo
except ImportError:
    env.Execute("$PYTHONEXE -m pip install GitPython")
    from git import Repo

build_flags = env['BUILD_FLAGS']

def parse_flags(path):
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

git_repo = Repo(os.getcwd(), search_parent_directories=True)
git_root = git_repo.git.rev_parse("--show-toplevel")
ExLRS_Repo = Repo(git_root)
sha = ExLRS_Repo.head.object.hexsha
build_flags.append("-DLATEST_COMMIT=0x"+sha[0]+",0x"+sha[1]+",0x"+sha[2]+",0x"+sha[3]+",0x"+sha[4]+",0x"+sha[5])

print("build flags: %s" % env['BUILD_FLAGS'])
