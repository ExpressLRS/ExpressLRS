Import("env")
import stlink
import shutil, os, subprocess, sys


def create_directory():
    #path = os.path.join(env['PROJECT_DIR'], "binaries")
    path = "binaries"
    try:
        os.makedirs(path, exist_ok=True)
    except OSError as err:
        if err.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise
    return path


def pack_bin(source, target, env):
    target = env['PIOENV'].lower()
    src_bin = source[0].rstr()
    if not os.path.exists("packer.py"):
        return
    if not "_stock" in target:
        return
    target = target.replace("_stock", "")
    flash_offset = 0
    build_flags = env['BUILD_FLAGS']
    for flag in build_flags:
        if "FLASH_OFFSET=" in flag:
            flag = flag.split("=")
            flash_offset = int(flag[-1], 16)
            break
    if not flash_offset:
        return
    if flash_offset < 0x08000000:
        flash_offset += 0x08000000
    base = create_directory()
    tgt_bin = os.path.join(base, target + "_elrs_bl.frk")
    print("Bootloader '%s' [offset: 0x%08X]" % (src_bin, flash_offset))
    print("  ==> '%s'" % tgt_bin)
    args = [sys.executable, "packer.py",
        "0x%08X" % flash_offset,
        src_bin, tgt_bin]
    subprocess.check_call(args)


def rename_bin(source, target, env):
    #print("source: %s" % source
    #print("target: %s" % target)
    #print("env: %s" % env.Dump())

    target = env['PIOENV'].lower()
    if "_stock" in target:
        return
    src_bin = source[0].rstr()
    base = create_directory()
    dst_bin = os.path.join(base, target+'_bootloader.bin')
    print("Copy bin: %s => %s" % (src_bin, dst_bin))
    shutil.copyfile(src_bin, dst_bin)


env.AddPostAction("buildprog", rename_bin)
env.AddPostAction("buildprog", pack_bin)

env.Replace(UPLOADCMD=stlink.on_upload)
