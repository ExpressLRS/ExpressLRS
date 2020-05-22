import random
import os

DEBUG = 0

CALC_MY_STEP = 1

FHSS_FREQS_HEAD = '''
#ifndef FHSS_FREQS_H_
#define FHSS_FREQS_H_

#include "platform.h"
#include <stdint.h>

'''

FHSS_FREQS_TAIL = '''
#endif /* FHSS_FREQS_H_ */
'''

RNG_MAX = 0x7FFF
seed = 0
use_local_rand = 1
rand_version = 1

# Fill the FHSSsequence with channel indices
# The 0 index is special - the 'sync' channel. The sync channel appears every
# syncInterval hops. The other channels are randomly distributed between the
# sync channels
SYNC_INTERVAL = 15

# Size of the FHSS sequence entries
#   TODO: modify biffer at some day (e.g 20 * SYNC_INTERVAL).
#         ElrsSyncPacket_s need to be updated to hold bigger than uin8_t value!!
NR_SEQUENCE_ENTRIES = 256

# returns values between 0 and 0x7FFF
# NB rngN depends on this output range, so if we change the
# behaviour rngN will need updating
def rng():
    global seed
    m = 2147483648
    a = 214013
    c = 2531011
    seed = (a * seed + c) % m
    return seed >> 16

def rngSeed(newSeed):
    global seed
    seed = newSeed
    random.seed(newSeed)

# returns 0 <= x < max where max <= 256
# (actual upper limit is higher, but there is one and I haven't
#  thought carefully about what it is)
def rngN(max):
    if use_local_rand:
        x = rng()
        result = (x * max) / RNG_MAX
        return int(result)
    return random.randrange(0, max, 1)


def print_fhss(vals, num_of_fhss):
    buffer = []
    print_lst = []
    i = 0
    for val in vals:
        print_lst.append("%2u" % val);

        if ((i + 1) % 16 == 0):
            buffer.append(", ".join(print_lst))
            print(" ".join(print_lst));
            print_lst = []
        i += 1
    if print_lst:
        buffer.append(", ".join(print_lst))
        print(" ".join(print_lst));

    for iter in range(num_of_fhss):
        print("  index %2d: %3u" % (iter, vals.count(iter)))
    return buffer


def FHSSrandomiseFHSSsequence_v1(num_of_fhss):

    def resetIsAvailable():
        l = [1] * num_of_fhss
        if SYNC_INTERVAL is not None:
            l[0] = 0
        return l

    isAvailable = resetIsAvailable()
    FHSSsequence = [0] * NR_SEQUENCE_ENTRIES
    prev = 0 # needed to prevent repeats of the same index

    # for each slot in the sequence table
    for i in range(NR_SEQUENCE_ENTRIES):
        if SYNC_INTERVAL is not None and ((i % SYNC_INTERVAL) == 0):
            # assign sync channel 0
            FHSSsequence[i] = 0
            prev = 0
        else:
            # pick one of the available channels. May need to loop to avoid repeats
            index = prev
            while (index == prev): # can't use index if it repeats the previous value
                c = rngN(isAvailable.count(1)) # returnc 0<c<nLeft
                # find the c'th entry in the isAvailable array
                # can skip 0 as that's the sync channel and is never available for normal allocation
                index = 1 if SYNC_INTERVAL is not None else 0
                found = 0
                while (index < num_of_fhss):
                    if (isAvailable[index]):
                        if (found == c):
                            break
                        found += 1
                    index += 1

                if (index == num_of_fhss):
                    # This should never happen
                    print("FAILED to find the available entry for '%s'!" % c)
                    # What to do? We don't want to hang as that will stop us getting to the wifi hotspot
                    # Use the sync channel
                    index = 0
                    break

            FHSSsequence[i] = index  # assign the value to the sequence array
            isAvailable[index] = 0   # clear the flag
            prev = index             # remember for next iteration
            if (isAvailable.count(1) == 0):
                # we've assigned all of the channels, so reset for next cycle
                isAvailable = resetIsAvailable()
    return FHSSsequence


def FHSSrandomiseFHSSsequence_v2(num_of_fhss):
    vals = [-1] * NR_SEQUENCE_ENTRIES
    index = -1
    for iter in range(NR_SEQUENCE_ENTRIES):
        # dont use same value if it is close to last three
        skip = [vals[iter - 3], vals[iter - 2], vals[iter - 1]]
        while (index in skip or
                ((index - 1) % NR_SEQUENCE_ENTRIES) in skip or
                ((index + 1) % NR_SEQUENCE_ENTRIES) in skip):
            index = rngN(num_of_fhss)
        vals[iter] = index
    return vals


def check_fhss_freqs_h(DOMAIN, MY_UID):

    MY_UID = [int(val, 16) for val in MY_UID.replace("-DMY_UID=", "").split(",")]
    DOMAIN = DOMAIN.replace("-D", "")

    FREQ_OFFSET_UID = sum(MY_UID[3:]) #(MY_UID[3] + MY_UID[4] + MY_UID[5])

    # Our table of FHSS frequencies. Define a regulatory domain to select the correct set for your location and radio
    if DOMAIN == "Regulatory_Domain_AU_433":
        FHSSfreqs = [
            433420000,
            433920000,
            434420000
        ]

    elif DOMAIN == "Regulatory_Domain_AU_915":
        FHSSfreqs = [
            915500000,
            916100000,
            916700000,
            917300000,

            917900000,
            918500000,
            919100000,
            919700000,

            920300000,
            920900000,
            921500000,
            922100000,

            922700000,
            923300000,
            923900000,
            924500000,

            925100000,
            925700000,
            926300000,
            926900000
        ]

    elif DOMAIN == "Regulatory_Domain_EU_868":
        '''
        s/* Frequency bands taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
        * Note: these frequencies fall in the license free H-band, but in combination with 500kHz
        * LoRa modem bandwidth used by ExpressLRS (EU allows up to 125kHz modulation BW only) they
        * will never pass RED certification and they are ILLEGAL to use.
        *
        * Therefore we simply maximize the usage of available spectrum so laboratory testing of the software won't disturb existing
        * 868MHz ISM band traffic too much.
        */
        '''
        FHSSfreqs = [
            863275000, # band H1, 863 - 865MHz, 0.1% duty cycle or CSMA techniques, 25mW EIRP
            863800000,
            864325000,
            864850000,
            865375000, # Band H2, 865 - 868.6MHz, 1.0% dutycycle or CSMA, 25mW EIRP
            865900000,
            866425000,
            866950000,
            867475000,
            868000000,
            868525000, # Band H3, 868.7-869.2MHz, 0.1% dutycycle or CSMA, 25mW EIRP
            869050000,
            869575000,
        ]

    elif DOMAIN == "Regulatory_Domain_EU_868_R9":
        # https://github.com/pascallanger/DIY-Multiprotocol-TX-Module/blob/4ae30dc3b049f18147d6e278817f7a5f425c2fb0/Multiprotocol/FrSkyR9_sx1276.ino#L43
        FHSSfreqs = [
            # FrSkyR9_freq_map_868
            859504640,
            860004352,
            860504064,
            861003776,
            861503488,
            862003200,
            862502912,
            863002624,
            863502336,
            864002048,
            864501760,
            865001472,
            865501184,
            866000896,
            866500608,
            867000320,
            867500032,
            867999744,
            868499456,
            868999168,
            869498880,
            869998592,
            870498304,
            870998016,
            871497728,
            871997440,
            872497152,
            # last two determined by FrSkyR9_step
            #0, 0
        ]

    elif DOMAIN == "Regulatory_Domain_EU_433":
        '''
         Frequency band G, taken from https://wetten.overheid.nl/BWBR0036378/2016-12-28#Bijlagen
         Note: As is the case with the 868Mhz band, these frequencies only comply to the license free portion
               of the spectrum, nothing else. As such, these are likely illegal to use.
        '''
        FHSSfreqs = [
            433100000,
            433925000,
            434450000
        ]

    elif DOMAIN == "Regulatory_Domain_FCC_915":
        # Very definitely not fully checked. An initial pass at increasing the hops
        FHSSfreqs = [
            903500000,
            904100000,
            904700000,
            905300000,

            905900000,
            906500000,
            907100000,
            907700000,

            908300000,
            908900000,
            909500000,
            910100000,

            910700000,
            911300000,
            911900000,
            912500000,

            913100000,
            913700000,
            914300000,
            914900000,

            915500000, # as per AU..
            916100000,
            916700000,
            917300000,

            917900000,
            918500000,
            919100000,
            919700000,

            920300000,
            920900000,
            921500000,
            922100000,

            922700000,
            923300000,
            923900000,
            924500000,

            925100000,
            925700000,
            926300000,
            926900000
        ]

    else:
        raise Exception("[error] No regulatory domain defined, please define one in common.h")
        return

    num_of_fhss = len(FHSSfreqs)
    print("Number of FHSS frequencies = %u" % num_of_fhss)

    macSeed = (MY_UID[2] << 24) + (MY_UID[3] << 16) + (MY_UID[4] << 8) + MY_UID[5]
    rngSeed(macSeed)

    uid_compare = ",".join(["0x%02X" % one for one in MY_UID])
    header = "// MY_UID=%s; DOMAIN=%s; RAND=%u.%u; SYNC_INTERVAL=%s; CALC_MY_STEP=%u\n" % (
        uid_compare, DOMAIN, rand_version, use_local_rand, SYNC_INTERVAL, CALC_MY_STEP)
    write_out = False
    try:
        with open(os.path.join('src', 'fhss_freqs.h'), "r") as _f:
            first = _f.readline()
            write_out = first != header
            _f.close()
    except IOError:
        write_out = True

    if write_out or DEBUG:
        print("write...")
        with open(os.path.join('src', 'fhss_freqs.h'), "w+") as _f:
            _f.write(header)
            _f.write(FHSS_FREQS_HEAD)

            _f.write("#define FREQ_OFFSET_UID (%u)\n" % FREQ_OFFSET_UID)
            _f.write("#define NR_SEQUENCE_ENTRIES (%u)\n" % NR_SEQUENCE_ENTRIES)

            my_step = 0
            if CALC_MY_STEP:
                my_step = sum(MY_UID[3:])
                my_step %= 8
            my_step |= 1
            _f.write("#define FHSS_MY_STEP (%u)\n\n" % (my_step))

            _f.write("/* Note: UID offset included */\n")
            _f.write('static uint32_t DRAM_ATTR FHSSfreqs[%u] = {\n' % num_of_fhss)
            freqs = ["    %u" % (freq + FREQ_OFFSET_UID) for freq in FHSSfreqs]
            _f.write(",\n".join(freqs))
            _f.write("\n};\n\n")

            _f.write('uint8_t DRAM_ATTR FHSSsequence[NR_SEQUENCE_ENTRIES] = {\n')
            if rand_version == 1:
                FHSSsequence = FHSSrandomiseFHSSsequence_v1(num_of_fhss)
            elif rand_version == 2:
                FHSSsequence = FHSSrandomiseFHSSsequence_v2(num_of_fhss)
            else:
                raise Exception("Unknown FHSS rand version!")
            hops = print_fhss(FHSSsequence, num_of_fhss)
            for line in hops:
                _f.write("    %s,\n" % line)
            _f.write("};\n")
            _f.write('/*\n Usage per freq:\n')
            for iter in range(num_of_fhss):
                _f.write("    index %2d: %3u\n" % (iter, FHSSsequence.count(iter)))
            _f.write('*/\n')
            _f.write(FHSS_FREQS_TAIL)
            _f.close()


def check_env_and_parse(build_flags):
    my_domain = ""
    my_uid = ""
    for flag in build_flags:
        if "Regulatory_Domain" in flag:
            my_domain = flag
        elif "MY_UID" in flag:
            my_uid = flag

    if my_domain and my_uid:
        check_fhss_freqs_h(my_domain, my_uid)
    else:
        raise Exception("Domain and UI are missing!")

if DEBUG:
    check_fhss_freqs_h("-DRegulatory_Domain_EU_868", "-DMY_UID=0xB8,0x27,0xEB,0x61,0x1D,0x38")
