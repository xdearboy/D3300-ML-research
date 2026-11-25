#!/usr/bin/env python3
"""Read-only audit of the D3300 C 1.02 A32 literal-pool veneers.

This recognises their instruction form and literal targets only.  It does not
assign T-Kernel API names or imply a callable ABI.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


ROM_BASE = 0x90020000
SCAN_BEGIN = 0x90225000
SCAN_END = 0x90226070
LDR_PC_LITERAL = 0xE51FF004  # ARM: ldr pc, [pc, #-4]
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def u32(data: bytes, address: int) -> int:
    return struct.unpack_from("<I", data, address - ROM_BASE)[0]


def classify_target(target: int) -> str:
    if 0x10000000 <= target <= 0x10100000:
        return "ram"
    if 0x90000000 <= target <= 0x92000000:
        return "rom"
    return "outside_expected_ranges"


def analyze(data: bytes) -> dict:
    if len(data) <= SCAN_END - ROM_BASE:
        raise ValueError("firmware is too short for veneer scan")
    stubs = []
    for address in range(SCAN_BEGIN, SCAN_END, 4):
        if u32(data, address) == LDR_PC_LITERAL:
            target = u32(data, address + 4)
            stubs.append({
                "address": f"0x{address:08X}",
                "literal_address": f"0x{address + 4:08X}",
                "target": f"0x{target:08X}",
                "target_class": classify_target(target),
            })
    classes = {name: sum(x["target_class"] == name for x in stubs)
               for name in ("ram", "rom", "outside_expected_ranges")}
    return {
        "image_sha256": hashlib.sha256(data).hexdigest(),
        "scan_range": [f"0x{SCAN_BEGIN:08X}", f"0x{SCAN_END:08X}"],
        "instruction_word": f"0x{LDR_PC_LITERAL:08X}",
        "veneer_count": len(stubs),
        "first_veneer": stubs[0]["address"] if stubs else None,
        "last_veneer": stubs[-1]["address"] if stubs else None,
        "target_classes": classes,
        "rom_target_veneers": [x for x in stubs if x["target_class"] == "rom"],
        "conclusion": (
            "These are A32 literal-pool branch veneers. Static inspection proves "
            "the instruction/literal targets, not an API name, argument ABI, or "
            "safety of calling a target."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    data = args.firmware.read_bytes()
    result = analyze(data)
    if args.verify:
        if result["image_sha256"] != EXPECTED_SHA256:
            raise SystemExit("unexpected firmware SHA-256")
        if (result["veneer_count"], result["first_veneer"], result["last_veneer"],
                result["target_classes"]) != (471, "0x90225004", "0x90226060",
                                                {"ram": 467, "rom": 4,
                                                 "outside_expected_ranges": 0}):
            raise SystemExit("veneer layout does not match D3300 C 1.02")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
