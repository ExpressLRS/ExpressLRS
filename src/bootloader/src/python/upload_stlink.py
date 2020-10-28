Import("env")
import stlink
import shutil, os

def rename_bin(source, target, env):
    #print("source: %s" % source
    #print("target: %s" % target)
    #print("env: %s" % env.Dump())
    src_bin = source[0].rstr()
    print("Source bin: %s" % src_bin)
    bin_path = os.path.dirname(src_bin)
    #dst_bin = os.path.join(bin_path, 'bootloader_'+env['PIOENV']+'.bin')
    dst_bin = os.path.join("..", 'bootloader_'+env['PIOENV']+'.bin')
    print("Destination bin: %s" % dst_bin)
    shutil.copyfile(src_bin, dst_bin)

env.AddPostAction("buildprog", rename_bin)

env.Replace(UPLOADCMD=stlink.on_upload)
