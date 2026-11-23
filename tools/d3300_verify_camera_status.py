#!/usr/bin/env python3
"""Verify CKRRCCameraStatus ring-buffer anchors in D3300 C 1.02 B-firmware.

The tool checks exact instruction/vtable bytes. It deliberately reports only
observed layout facts; it does not assign photographic meanings to bit fields.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any, Iterable

ROM_BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"

ANCHORS = (
    {
        "name": "constructor_clears_ring_heads",
        "address": 0x9019A34E,
        "bytes": "c0f88040c0f88440",
        "observation": "stores zero to object +0x80 and +0x84",
    },
    {
        "name": "ring_initializer",
        "address": 0x9019A3AE,
        "bytes": "00f130032946c0f88430",
        "observation": "forms object +0x30 and stores it at object +0x84",
    },
    {
        "name": "snapshot_rotation",
        "address": 0x9019A4EC,
        "bytes": "00f18404d1b04ff0704720684ff0140200682060c5684168",
        "observation": "uses object +0x84, advances the node, then loads node +0x0C and linked-node +0x0C",
    },
    {
        "name": "transition_byte_0x8f",
        "address": 0x9019A558,
        "bytes": "28684ff0010931684ff00008c0f34012c1f340119142",
        "observation": "compares bit 5 in two snapshots before writing transition byte +0x8F",
    },
    {
        "name": "transition_byte_0x8c",
        "address": 0x9019A6FC,
        "bytes": "28683168c0f34022c1f34021914207d0800502d584f88c90",
        "observation": "compares bit 9 in two snapshots and writes code 1 to +0x8C on one transition",
    },
    {
        "name": "status_snapshot_getter",
        "address": 0x90350DD0,
        "bytes": "d0f88400c0687047",
        "observation": "returns *( *(object +0x84) +0x0C )",
    },
    {
        "name": "vtable_slot_plus_0x14",
        "address": 0x9151F704,
        "bytes": "d10d3590",
        "observation": "vtable slot +0x14 is Thumb target 0x90350DD1",
    },
    {
        "name": "info_disp_data_global",
        "address": 0x9013BED4,
        "bytes": "14274910",
        "observation": "literal used by the constructor call resolves to RAM 0x10492714",
    },
    {
        "name": "info_disp_data_constructor",
        "address": 0x9019CC70,
        "bytes": "be49c0f8b042c0f8b442c0f8b8420160",
        "observation": "clears +0x2B0/+0x2B4/+0x2B8 and installs the CKRRCInfoDispData vtable",
    },
    {
        "name": "info_disp_data_ring_initializer",
        "address": 0x9019CCBE,
        "bytes": "00f504722946c0f8b4221ce0",
        "observation": "forms object +0x210, stores it at +0x2B4 and starts the 11-node ring loop",
    },
    {
        "name": "info_disp_data_getter",
        "address": 0x90350DDE,
        "bytes": "d0f8b402c0687047",
        "observation": "returns *( *(object +0x2B4) +0x0C )",
    },
    {
        "name": "info_disp_data_vtable",
        "address": 0x9151F73C,
        "bytes": "6829509193cc1990adcc1990bbcc199071302b9077302b90df0d35904bcd199067cd1990",
        "observation": "RTTI 17CKRRCInfoDispData followed by its primary vtable",
    },
)


def check_anchors(data: bytes, base: int = ROM_BASE,
                  anchors: Iterable[dict[str, Any]] = ANCHORS) -> list[dict[str, Any]]:
    results = []
    for anchor in anchors:
        expected = bytes.fromhex(anchor["bytes"])
        offset = anchor["address"] - base
        actual = data[offset:offset + len(expected)] if offset >= 0 else b""
        results.append({
            "name": anchor["name"],
            "address": f"0x{anchor['address']:08X}",
            "expected_bytes": expected.hex(),
            "actual_bytes": actual.hex(),
            "match": actual == expected,
            "observation": anchor["observation"],
        })
    return results


def analyze(data: bytes) -> dict[str, Any]:
    digest = hashlib.sha256(data).hexdigest()
    anchors = check_anchors(data)
    return {
        "firmware": "Nikon D3300 C 1.02 B-firmware",
        "rom_base": f"0x{ROM_BASE:08X}",
        "sha256": digest,
        "expected_sha256": EXPECTED_SHA256,
        "sha256_match": digest == EXPECTED_SHA256,
        "all_anchors_match": all(item["match"] for item in anchors),
        "conclusion": (
            "CKRRCCameraStatus +0x84 selects a ring node whose +0x0C field points "
            "to an 8-byte bitfield snapshot. Bytes +0x8C..+0x98 are transition "
            "indicators derived by comparing snapshots, not verified exposure fields. "
            "A separate CKRRCInfoDispData object at 0x10492714 maintains 11 snapshots "
            "of 52 bytes and is the stronger candidate for displayed shooting data."
        ),
        "anchors": anchors,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="decoded b860_102.bin")
    parser.add_argument("--dump-json", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()

    report = analyze(args.binary.read_bytes())
    if args.dump_json:
        args.dump_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    else:
        print(json.dumps(report, indent=2))

    if args.verify and not (report["sha256_match"] and report["all_anchors_match"]):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
