#!/usr/bin/env python3
"""Classify verified menu/display seams without claiming camera-safe hooks."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

BASE = 0x90020000
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def raw(data: bytes, address: int, size: int) -> bytes:
    offset = address - BASE
    return data[offset:offset + size]


def analyze(data: bytes) -> dict:
    active_literal = struct.unpack("<I", raw(data, 0x9002A928, 4))[0]
    veneer_instruction = struct.unpack("<I", raw(data, 0x90225EA4, 4))[0]
    veneer_target = struct.unpack("<I", raw(data, 0x90225EA8, 4))[0]
    seams = [
        {
            "name": "active_view_pointer_writer",
            "anchor": "0x9002A6A8",
            "evidence_match": raw(data, 0x9002A6A8, 6).hex() == "9f49a8680860",
            "ram_address": f"0x{active_literal:08X}",
            "classification": "observation_candidate",
            "limit": "ROM proves the writer; lifetime, synchronization and safe external reads are unproven",
        },
        {
            "name": "menu_action_router",
            "anchor": "0x90254E14",
            "evidence_match": raw(data, 0x90254E14, 4).hex() == "1d2d2bd2",
            "classification": "control_flow_only",
            "limit": "logical IDs and vtable slots do not prove physical buttons or a safe interception contract",
        },
        {
            "name": "osd_primitive_appender",
            "anchor": "0x903111BC",
            "evidence_match": raw(data, 0x903111BC, 4).hex() == "70b51068",
            "classification": "internal_call_only",
            "limit": "record construction is proven; capacity, ownership and external caller safety are unproven",
        },
        {
            "name": "pre_output_veneer",
            "anchor": "0x90225EA4",
            "evidence_match": veneer_instruction == 0xE51FF004,
            "runtime_target": f"0x{veneer_target:08X}",
            "classification": "dynamic_runtime_boundary",
            "limit": "service name, ABI, side effects and output implementation are absent from ROM",
        },
    ]
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "seams": seams,
        "safe_hook_count": 0,
        "conclusion": "no camera-safe code-insertion or rendering seam is established by static ROM evidence",
    }


def verify(result: dict) -> bool:
    return (
        result["sha256"] == SHA256
        and all(item["evidence_match"] for item in result["seams"])
        and result["seams"][0]["ram_address"] == "0x10123844"
        and result["seams"][3]["runtime_target"] == "0x10099244"
        and result["safe_hook_count"] == 0
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    result = analyze(args.firmware.read_bytes())
    if args.verify and not verify(result):
        raise SystemExit("extension-seam anchors do not match D3300 C 1.02")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
