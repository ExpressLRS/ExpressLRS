"""Validate the appended ER6 hardware configuration and print provenance JSON."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src" / "python"))
sys.path.insert(0, str(ROOT / "src" / "python" / "external" / "esptool"))
from UnifiedConfiguration import findFirmwareEnd


def verify(path):
    image = Path(path)
    blob = image.read_bytes()
    with image.open("rb") as stream:
        end = findFirmwareEnd(stream)
        stream.seek(end)
        product = stream.read(128).split(b"\0")[0].decode()
        lua_name = stream.read(16).split(b"\0")[0].decode()
        options = json.loads(stream.read(512).split(b"\0")[0])
        hardware = json.loads(stream.read(2048).split(b"\0")[0])
    assert product == "RadioMaster ER6 2.4GHz Diversity+6xPWM RX", product
    assert lua_name == "RM ER6", lua_name
    assert hardware["serial_rx"] == 3 and hardware["serial_tx"] == 1, hardware
    assert hardware["pwm_outputs"] == [14, 12, 15, 2, 4, 9], hardware
    assert not ({1, 3} & set(hardware["pwm_outputs"]))
    assert not any(key in options for key in ("uid", "wifi-ssid", "wifi-password")), "Private build options present"
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    hardware_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT / "src" / "hardware", text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip())
    # Upstream elrs_helpers.get_git_version embeds six characters and a NUL.
    assert source[:6].encode() + b"\0" in blob, "Binary source revision does not match this checkout"
    assert not dirty, "Commit source changes before producing a release manifest"
    assert hardware_commit == "c8bb70d6ad08da08381fc898adf29b8ae40eae12", "Unexpected hardware revision"
    targets = json.loads(subprocess.check_output(
        ["git", "show", hardware_commit + ":targets.json"], cwd=ROOT / "src" / "hardware", text=True))
    target = targets["radiomaster"]["rx_2400"]["er6"]
    layout = json.loads(subprocess.check_output(
        ["git", "show", hardware_commit + ":RX/" + target["layout_file"]], cwd=ROOT / "src" / "hardware", text=True))
    layout.update(target.get("overlay", {}))
    assert hardware == layout, "Binary hardware configuration does not match the pinned revision"
    return {
        "file": image.name, "bytes": len(blob), "sha256": hashlib.sha256(blob).hexdigest(),
        "product": product, "lua_name": lua_name, "source_commit": source, "source_dirty": dirty,
        "base_commit": "a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6", "hardware_commit": hardware_commit,
        "target": "radiomaster.rx_2400.er6", "serial_rx": 3, "serial_tx": 1,
        "pwm_outputs": hardware["pwm_outputs"], "hardware_validation": "pending",
        "smart_bus_gpio": 1, "throttle_channel": 3,
        "build_command": "pio run -c platformio.er6.ini -e Unified_ESP32_2400_RX_via_UART",
    }


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify-er6-image.py firmware.bin")
    print(json.dumps(verify(sys.argv[1]), indent=2))
