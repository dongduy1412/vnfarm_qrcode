"""Copy a successful PlatformIO build and record offsets/hashes; never flash."""
import hashlib
import json
import shutil
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--env", choices=["s3tim", "s3tim_local"], default="s3tim")
args = parser.parse_args()
BUILD = ROOT / ".pio" / "build" / args.env
DIST = ROOT / "dist" if args.env == "s3tim" else ROOT / "dist" / args.env
FRAMEWORK = Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32"
images = [
    ("0x0000", BUILD / "bootloader.bin"),
    ("0x8000", BUILD / "partitions.bin"),
    ("0xe000", FRAMEWORK / "tools" / "partitions" / "boot_app0.bin"),
    ("0x10000", BUILD / "firmware.bin"),
]
DIST.mkdir(exist_ok=True)
manifest = {"chip": "esp32s3", "flash_size": "16MB", "environment": args.env, "images": []}
for offset, source in images:
    destination = DIST / source.name
    shutil.copy2(source, destination)
    binary = destination.read_bytes()
    manifest["images"].append({"offset": offset, "file": source.name, "bytes": len(binary),
                                "sha256": hashlib.sha256(binary).hexdigest()})
(DIST / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(json.dumps(manifest, indent=2))

