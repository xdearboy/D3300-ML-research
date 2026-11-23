#!/usr/bin/env python3
"""Verify and extract the CKRRCInfoDispDataGrp property map.

This is a read-only structural verifier.  The numeric property IDs are emitted
without photographic names until a formatter/consumer path proves a meaning.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any

ROM_BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
TABLE_ADDRESS = 0x914ED324
ENTRY_COUNT = 79
OUTPUT_ADDRESS = 0x104B9044
OUTPUT_SIZE = 0x13C

ANCHORS = (
    (0x9019DE44, "01a94e484ff49e72d6f05ceedaf8c002", "copies 0x13C bytes from stack+4"),
    (0x9019DF80, "44904b10", "copy destination is RAM 0x104B9044"),
    (0x903513AC, "4f200860014810607047000024d34e91", "group getter returns 79 entries at 0x914ED324"),
    (0x915040E8, "08b45491d6d35091501a5091", "RTTI record for CKRRCInfoDispDataGrp"),
    (0x91528268, "e8405091ad133590d3ff2a90d5ff2a90", "RTTI pointer and group vtable"),
)


def _slice(data: bytes, address: int, size: int) -> bytes:
    offset = address - ROM_BASE
    return data[offset:offset + size] if offset >= 0 else b""


def extract_entries(data: bytes) -> list[dict[str, Any]]:
    entries = []
    for index in range(ENTRY_COUNT):
        address = TABLE_ADDRESS + index * 12
        property_id, size, pointer = struct.unpack("<III", _slice(data, address, 12))
        entries.append({
            "index": index,
            "table_address": f"0x{address:08X}",
            "ram_address": f"0x{pointer:08X}",
            "output_offset": f"0x{pointer - OUTPUT_ADDRESS:03X}",
            "property_id": f"0x{property_id:04X}",
            "size": size,
            "semantic_name": None,
        })
    return entries


def analyze(data: bytes) -> dict[str, Any]:
    checks = []
    for address, expected_hex, observation in ANCHORS:
        expected = bytes.fromhex(expected_hex)
        actual = _slice(data, address, len(expected))
        checks.append({
            "address": f"0x{address:08X}",
            "expected_bytes": expected.hex(),
            "actual_bytes": actual.hex(),
            "match": actual == expected,
            "observation": observation,
        })

    entries = extract_entries(data)
    layout_match = all(
        int(entry["ram_address"], 16) == OUTPUT_ADDRESS + entry["index"] * 4
        and entry["size"] in (1, 4)
        for entry in entries
    )
    digest = hashlib.sha256(data).hexdigest()
    return {
        "firmware": "Nikon D3300 C 1.02 B-firmware",
        "sha256": digest,
        "sha256_match": digest == EXPECTED_SHA256,
        "anchors_match": all(item["match"] for item in checks),
        "layout_match": layout_match,
        "group": "20CKRRCInfoDispDataGrp",
        "output_ram": f"0x{OUTPUT_ADDRESS:08X}",
        "output_size": f"0x{OUTPUT_SIZE:X}",
        "entry_count": ENTRY_COUNT,
        "semantic_status": "numeric IDs verified; photographic meanings not yet assigned",
        "anchors": checks,
        "entries": entries,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="decoded b860_102.bin")
    parser.add_argument("--dump-json", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    report = analyze(args.binary.read_bytes())
    rendered = json.dumps(report, indent=2) + "\n"
    if args.dump_json:
        args.dump_json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    if args.verify and not (
        report["sha256_match"] and report["anchors_match"] and report["layout_match"]
    ):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
