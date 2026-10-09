"""Metadata/provenance regressions using synthetic image containers (not flashable)."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("verify_image", Path(__file__).with_name("verify-er6-image.py"))
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)
TARGETS = json.loads((verifier.ROOT / "src/hardware/targets.json").read_text())
TARGET = TARGETS["radiomaster"]["rx_2400"]["er6"]
LAYOUT = json.loads((verifier.ROOT / "src/hardware/RX" / TARGET["layout_file"]).read_text())
HARDWARE = {**LAYOUT, **TARGET["overlay"]}
SOURCE = "abcdef012345678901234567890123456789012345"
HARDWARE_SHA = "c8bb70d6ad08da08381fc898adf29b8ae40eae12"
UPSTREAM_BASE = "8c51826de3ae95fa02002d813b120c677bdf122a"


def git_result(args, cwd, text):
    if args[1] == "merge-base":
        return UPSTREAM_BASE + "\n"
    if args[1] == "status":
        return ""
    if args[1] == "show":
        return json.dumps(TARGETS if args[2].endswith("targets.json") else LAYOUT)
    return (HARDWARE_SHA if Path(cwd).name == "hardware" else SOURCE) + "\n"


def container(path, source, hardware):
    payload = source.encode() + b"\0"
    header = bytearray(24)
    header[0:2] = bytes([0xE9, 1])
    blob = bytes(header) + struct.pack("<II", 0x3F400020, len(payload)) + payload
    end = ((len(blob) + 16) & ~15) + 32
    blob += b"\0" * (end - len(blob))
    for value, size in ((TARGET["product_name"], 128), ("RM ER6", 16), ("{}", 512), (json.dumps(hardware), 2048)):
        blob += value.encode().ljust(size, b"\0")
    path.write_bytes(blob)


class ImageProvenance(unittest.TestCase):
    def check(self, source, hardware):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "fixture.bin"
            container(image, source, hardware)
            with patch.object(verifier.subprocess, "check_output", side_effect=git_result):
                return verifier.verify(image)

    def test_matching_revision_and_complete_hardware(self):
        result = self.check(SOURCE[:6], HARDWARE)
        self.assertEqual(SOURCE, result["source_commit"])
        self.assertEqual(HARDWARE_SHA, result["hardware_commit"])
        self.assertEqual(UPSTREAM_BASE, result["base_commit"])
        self.assertEqual(11, result["smart_protocol_id"])

    def test_old_revision_must_not_receive_current_provenance(self):
        with self.assertRaisesRegex(AssertionError, "revision"):
            self.check("1111111", HARDWARE)

    def test_other_hardware_fields_are_bound_to_revision(self):
        changed = {**HARDWARE, "radio_nss": HARDWARE["radio_nss"] + 1}
        with self.assertRaisesRegex(AssertionError, "hardware"):
            self.check(SOURCE[:6], changed)


if __name__ == "__main__":
    unittest.main()
