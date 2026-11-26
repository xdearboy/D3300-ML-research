#!/usr/bin/env python3
"""Verify ROM anchors that terminate at runtime-populated D3300 RAM state."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def word(data: bytes, address: int) -> int:
    return struct.unpack_from("<I", data, address - BASE)[0]


def analyze(data: bytes) -> dict:
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "boundaries": [
            {
                "name": "input_channel_callback_table",
                "rom_literal": "0x9011E188",
                "ram_base": f"0x{word(data, 0x9011E188):08X}",
                "runtime_field": "base + 0x18",
                "conclusion": "post-boot callback target is not present in the ROM image",
            },
            {
                "name": "info_property_subscriber_registry",
                "rom_literal": "0x90169B24",
                "loaded_ram": f"0x{word(data, 0x90169B24):08X}",
                "derived_base": f"0x{word(data, 0x90169B24) - 0x70:08X}",
                "entry_stride": 8,
                "conclusion": "subscriber callbacks require post-boot RAM contents",
            },
            {
                "name": "pre_output_veneer",
                "veneer": "0x90225EA4",
                "instruction": f"0x{word(data, 0x90225EA4):08X}",
                "ram_target": f"0x{word(data, 0x90225EA8):08X}",
                "conclusion": "ROM proves the branch veneer, not the runtime target ABI",
            },
            {
                "name": "cguicdraw_static_object",
                "rom_literal": "0x9012ADB4",
                "ram_object": f"0x{word(data, 0x9012ADB4):08X}",
                "conclusion": "constructor identity is static; selected output objects are runtime state",
            },
        ],
    }


def verify(result: dict) -> bool:
    b = result["boundaries"]
    return (
        result["sha256"] == EXPECTED_SHA256
        and b[0]["ram_base"] == "0x10319D2C"
        and b[1]["loaded_ram"] == "0x10138B84"
        and b[1]["derived_base"] == "0x10138B14"
        and b[2]["instruction"] == "0xE51FF004"
        and b[2]["ram_target"] == "0x10099244"
        and b[3]["ram_object"] == "0x10383580"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    result = analyze(args.firmware.read_bytes())
    if args.verify and not verify(result):
        raise SystemExit("runtime-boundary anchors do not match D3300 C 1.02")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
