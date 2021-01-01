import os
import sys


def gen_elrs(source, target, env):
    source_bin = source[0]
    sys.stdout.write("Source bin: %s \n" % source_bin)
    bin_path = os.path.dirname(source_bin.rstr())
    bin_target = os.path.join(bin_path, 'firmware.elrs')
    with open(bin_target, "wb+") as _out:
        _out.write(source_bin.get_contents())
        _out.close()
    sys.stdout.write("\n")
    sys.stdout.write("=====================================================================================================================================\n")
    sys.stdout.write("|| !!! Copy %s to SD card and choose flash ext. ELRS !!! ||\n" % bin_target)
    sys.stdout.write("=====================================================================================================================================\n")
    sys.stdout.write("\n")
    sys.stdout.flush()
