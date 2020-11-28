import os
import sys


def gen_multi_bin(source, target, env):
    target_bin = target[0]
    bin_path = os.path.dirname(target_bin.rstr())
    bin_target = os.path.join(bin_path, 'elrs.opentx.bin')
    with open(bin_target, "wb+") as _out:
        _out.write(target_bin.get_contents())
        # append version information (24bytes) to end of the bin file
        _out.write("multi-x00000b81-01030073".encode('utf-8'))
        _out.close()
    sys.stdout.write("Copy %s to SD card and choose flash external multi" % bin_target)
    sys.stdout.flush()


def gen_elrs(source, target, env):
    if not "_stock" in env['PIOENV']:
        return
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


def gen_frsky(source, target, env):
    if "_stock" in env['PIOENV']:
        return
    sys.stdout.write("\n")
    sys.stdout.write("\n")
    sys.stdout.write("Building flashable .frk file...\n")
    target_bin = source[0] # target[0]
    sys.stdout.write("Source bin: %s \n" % target_bin)
    bin_path = os.path.dirname(target_bin.rstr())
    bin_target = os.path.join(bin_path, 'elrs.frk')
    with open(bin_target, "wb+") as _out:
        bin_content = target_bin.get_contents()
        # append FrSky header (16bytes)
        '''
            struct FrSkyFirmwareInformation_no_pack {
                uint32_t fourcc; = 0x4B535246
                uint8_t headerVersion; = 1
                uint8_t firmwareVersionMajor;
                uint8_t firmwareVersionMinor;
                uint8_t firmwareVersionRevision;
                uint32_t size; == len(bin_content)
                uint8_t productFamily;
                uint8_t productId;
                uint16_t crc;
            };
        '''
        sys.stdout.write("Bin size: %u \n" % len(bin_content))
        _out.write(b"\x46\x52\x53\x4B") # fourcc
        _out.write(b"\x01") # header version
        _out.write(b"\x00\x00\x00")  # fw versions
        size = len(bin_content) + 16
        _out.write(bytearray(
            [size & 0xFF, size >> 8 & 0xFF, size >> 16 & 0xFF, size >> 24 & 0xFF]))
        _out.write(b"\x00\x00") # productFamily, productId
        _out.write(b"\x00\x00") # crc
        _out.write(bin_content)
        _out.close()
    sys.stdout.write("\n")
    sys.stdout.write("=====================================================================================================================================\n")
    sys.stdout.write("|| !!! Copy %s to SD card and choose flash external in order to flash via OpenTX !!! ||\n" % bin_target)
    sys.stdout.write("=====================================================================================================================================\n")
    sys.stdout.write("\n")
    sys.stdout.flush()
