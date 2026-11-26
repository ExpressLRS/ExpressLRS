#!/usr/bin/env python3
"""Build the card-ready receiver images and manifest that the TX fetches for
"update receivers from the radio's SD card".

  rx_sd_release.py FIRMWARE_DIR OUT_DIR [--version V] [--sha SHA6]

FIRMWARE_DIR is the CI bundle layout (dist/firmware): <domain>/<env>/firmware.bin,
where <env> is the PlatformIO env name with the "_via_*" suffix cut off
(e.g. FCC/Unified_ESP32_2400_RX/firmware.bin).

Writes OUT_DIR/manifest.json and OUT_DIR/<key>/<version>.bin.gz, where <key> is
<platform>-<radio> (the same key the RX computes in RxUpdateReceiver*.cpp).

Image content (identical to the bench strip_trailer.py, then gzip with mtime=0):
  ESP32 family: the app image with the blank options trailer cut off.
  ESP8285:      the app image followed by an erased (0xFF) trailer; the RX can't
                append one itself, and an erased one makes the new firmware see
                "no trailer" and keep the old settings.

--version defaults to what the firmware reports as GIT_VERSION and --sha to the
first 6 characters of HEAD (both from elrs_helpers.get_git_version()).
"""
import argparse
import gzip
import hashlib
import json
import os
import re
import struct
import sys

TRAILER_SIZE = 128 + 16 + 512 + 2048  # product name + device name + options + hardware

# CI builds each 2400 / LR1121 env twice: FCC/ (ISM / FCC_915) and LBT/ (compiled
# with Regulatory_Domain_EU_CE_2400). LBT is a separate build, not a trailer setting,
# so it gets its own key suffix. Envs CI builds once (900 MHz, LR2021) are FCC/ only;
# their 900 MHz domain is chosen in the trailer.
DOMAIN_SUFFIX = {'FCC': '', 'LBT': '-lbt'}

ENV_RE = re.compile(r'^Unified_(ESP32C3|ESP32S3|ESP32|ESP8285)_(2400|900|LR1121|LR2021)_RX$')
PLATFORMS = {'ESP32': 'esp32', 'ESP32S3': 'esp32s3', 'ESP32C3': 'esp32c3', 'ESP8285': 'esp8285'}
RADIOS = {'2400': '2400', '900': '900', 'LR1121': 'lr1121', 'LR2021': 'lr2021'}


def env_to_key(env):
    """'Unified_ESP32_2400_RX' -> ('esp32-2400', is_esp8266), or None if not an RX we ship."""
    m = ENV_RE.match(env)
    if not m:
        return None
    platform, radio = m.groups()
    if platform == 'ESP8285' and radio not in ('2400', '900'):
        return None
    return '%s-%s' % (PLATFORMS[platform], RADIOS[radio]), platform == 'ESP8285'


MAX_VERSION_LEN = 24  # SD paths are limited to 64 chars; longest key is 18 (esp32s3-lr1121-lbt)


def sanitize_version(v, sha6):
    """Make a version string safe as a file name (letters, digits, '.', '-', '_'),
    at most MAX_VERSION_LEN long; longer ones become <first 17 chars>-<sha6>."""
    v = re.sub(r'[^A-Za-z0-9._-]', '_', v)
    v = re.sub(r'^\.+', lambda m: '_' * len(m.group(0)), v)  # no '.', '..' or hidden names
    v = v or 'unknown'
    if len(v) > MAX_VERSION_LEN:
        v = v[:MAX_VERSION_LEN - 1 - len(sha6)] + '-' + sha6
    return v


def image_end(d, start, header):
    """Same walk as the bootloaders / EspClass::getSketchSize()."""
    off = start + header
    for _ in range(d[start + 1]):
        _, ln = struct.unpack_from('<II', d, off)
        off += 8 + ln
    return off


def make_card_image(d, esp8266):
    if esp8266:
        # eboot sits in the first 4 KB; the app image header is at 0x1000
        end = (image_end(d, 0x1000, 8) + 16) & ~15
        return d[:end] + b'\xff' * TRAILER_SIZE
    end = (image_end(d, 0, 24) + 1 + 15) & ~15  # 24 B extended header; checksum byte, pad to 16
    if d[23] == 1:
        end += 32  # appended SHA-256
    return d[:end]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('firmware_dir')
    ap.add_argument('out_dir')
    ap.add_argument('--version')
    ap.add_argument('--sha')
    args = ap.parse_args()

    if args.version is None or args.sha is None:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import elrs_helpers
        ver = elrs_helpers.get_git_version()
        args.version = args.version or ver['version']
        args.sha = args.sha or ver['sha']
    sha6 = args.sha[:6].lower()
    if not re.fullmatch(r'[0-9a-f]{6}', sha6):
        sys.exit('bad sha: %r' % args.sha)
    version = sanitize_version(args.version, sha6)

    # one env per key: the CI only builds *_via_UART, which is the same binary as _via_WIFI/_via_BetaflightPassthrough
    images = {}
    for domain, suffix in DOMAIN_SUFFIX.items():
        ddir = os.path.join(args.firmware_dir, domain)
        if not os.path.isdir(ddir):
            continue
        for env in sorted(os.listdir(ddir)):
            kt = env_to_key(env)
            src = os.path.join(ddir, env, 'firmware.bin')
            if kt is None or not os.path.isfile(src):
                continue
            key, esp8266 = kt[0] + suffix, kt[1]
            with open(src, 'rb') as f:
                data = gzip.compress(make_card_image(f.read(), esp8266), 9, mtime=0)
            rel = '%s/%s.bin.gz' % (key, version)
            os.makedirs(os.path.join(args.out_dir, key), exist_ok=True)
            with open(os.path.join(args.out_dir, rel), 'wb') as f:
                f.write(data)
            images[key] = {'version': version, 'file': rel, 'size': len(data),
                           'sha256': hashlib.sha256(data).hexdigest()}
            print('%-20s %-40s -> %s (%d bytes)' % (key, os.path.relpath(src, args.firmware_dir), rel, len(data)))

    if not images:
        sys.exit('no RX firmware found under %s' % args.firmware_dir)
    os.makedirs(args.out_dir, exist_ok=True)
    with open(os.path.join(args.out_dir, 'manifest.json'), 'w', newline='\n') as f:
        json.dump({'release': version, 'commit': sha6, 'images': images}, f, indent=1, sort_keys=False)
        f.write('\n')
    print('%d images, release %s, commit %s' % (len(images), version, sha6))


if __name__ == '__main__':
    main()
