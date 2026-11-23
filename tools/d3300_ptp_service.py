#!/usr/bin/env python3
"""Find raw candidate values related to Nikon PTP research.

This is a byte-pattern scanner, not a PTP dispatcher analyzer. A matching
little-endian halfword can be an instruction, data, pixel value or unrelated
constant. Names below come from other Nikon models and are hypotheses until a
D3300 dispatch path and handler are established by disassembly.

Candidate values:
  - 0x94FE: OP_SERVICE_WRAP_SEND
  - 0x94FF: OP_SERVICE_WRAP_RECV
  - 0xF001: Sub-op unlock service access
  - 0xF036: Sub-op arbitrary physical memory read
  - 0xF037: Sub-op arbitrary physical memory write
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from typing import Any, Dict, List, Optional, Tuple

ROM_BASE = 0x90020000

VENDOR_OPCODES = {
    0x9001: "PTP_OC_NIKON_GET_EVENT_DATA",
    0x9002: "PTP_OC_NIKON_DEVICE_READY",
    0x94FE: "CANDIDATE_SERVICE_WRAP_SEND",
    0x94FF: "CANDIDATE_SERVICE_WRAP_RECV",
}

SERVICE_SUBOPCODES = {
    0xF001: "CANDIDATE_ENABLE_SERVICE_ACCESS",
    0xF036: "CANDIDATE_MEMORY_READ",
    0xF037: "CANDIDATE_MEMORY_WRITE",
}


def load_binary(path: str) -> Tuple[bytes, int]:
    with open(path, "rb") as f:
        data = f.read()

    # Detect ELF header
    if data.startswith(b"\x7fELF"):
        # Text segment starts at 0x100 in d3300_analysis.elf
        base = ROM_BASE
        content = data[0x100:]
        return content, base

    return data, ROM_BASE


def scan_ptp_vendor_opcodes(data: bytes, base: int = ROM_BASE) -> List[Dict[str, Any]]:
    results: List[Dict[str, Any]] = []

    for opcode, name in sorted(VENDOR_OPCODES.items()):
        pattern = struct.pack("<H", opcode)
        idx = 0
        while True:
            pos = data.find(pattern, idx)
            if pos == -1:
                break
            rom_addr = base + pos
            # Check context: handler pointer nearby in dispatch table
            func_after: Optional[int] = None
            func_before: Optional[int] = None

            if pos + 6 <= len(data):
                val = struct.unpack("<I", data[pos + 2 : pos + 6])[0]
                if 0x90020000 <= val <= 0x91600000 and (val & 1):
                    func_after = val

            if pos >= 4:
                val = struct.unpack("<I", data[pos - 4 : pos])[0]
                if 0x90020000 <= val <= 0x91600000 and (val & 1):
                    func_before = val

            results.append({
                "opcode": f"0x{opcode:04X}",
                "name": name,
                "rom_offset": f"0x{rom_addr:08X}",
                "target_func_after": f"0x{func_after:08X}" if func_after else None,
                "target_func_before": f"0x{func_before:08X}" if func_before else None,
                "evidence": "raw_halfword_occurrence",
            })
            idx = pos + 2

    return results


def scan_service_subopcodes(data: bytes, base: int = ROM_BASE) -> List[Dict[str, Any]]:
    results: List[Dict[str, Any]] = []

    for subop, name in sorted(SERVICE_SUBOPCODES.items()):
        pattern = struct.pack("<H", subop)
        idx = 0
        while True:
            pos = data.find(pattern, idx)
            if pos == -1:
                break
            rom_addr = base + pos
            results.append({
                "subop": f"0x{subop:04X}",
                "name": name,
                "rom_offset": f"0x{rom_addr:08X}",
                "evidence": "raw_halfword_occurrence",
            })
            idx = pos + 2

    return results


def analyze_firmware_ptp(data: bytes, base: int = ROM_BASE) -> Dict[str, Any]:
    vendor_ops = scan_ptp_vendor_opcodes(data, base)
    subops = scan_service_subopcodes(data, base)

    # Filter clustered service subops around 0x90033BDC..0x90033C56
    service_cluster = [
        s for s in subops if 0x90033000 <= int(s["rom_offset"], 16) <= 0x90034000
    ]

    return {
        "warning": "Raw halfword occurrences only; no D3300 PTP command semantics are verified.",
        "rom_base": f"0x{base:08X}",
        "total_size": len(data),
        "vendor_opcodes_found": len(vendor_ops),
        "service_subops_found": len(subops),
        "service_cluster_0x90033000": service_cluster,
        "vendor_opcodes": vendor_ops,
        "service_subops": subops,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", help="Path to b860_102.bin or d3300_analysis.elf")
    parser.add_argument("--dump-json", "-o", help="Save structured JSON report to path")
    parser.add_argument(
        "--verify",
        action="store_true",
        help="legacy structural sanity check only; does not verify PTP semantics",
    )
    args = parser.parse_args()

    data, base = load_binary(args.binary)
    report = analyze_firmware_ptp(data, base)

    if args.verify:
        if report["vendor_opcodes_found"] == 0:
            sys.stderr.write("Candidate check failed: no raw vendor values discovered\n")
            return 1
        if len(report["service_cluster_0x90033000"]) == 0:
            sys.stderr.write("Candidate check failed: no raw values in the selected ROM window\n")
            return 1

    if args.dump_json:
        with open(args.dump_json, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)
        print(f"Report written to {args.dump_json}")
    else:
        print(report["warning"])
        print(f"Vendor-value occurrences: {report['vendor_opcodes_found']}")
        print(f"Sub-operation-value occurrences: {report['service_subops_found']}")
        print(f"Occurrences in 0x90033000 window: {len(report['service_cluster_0x90033000'])}")
        for item in report["service_cluster_0x90033000"]:
            print(f"  {item['subop']} ({item['name']}) at {item['rom_offset']}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
