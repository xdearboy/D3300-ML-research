#!/usr/bin/env python3
"""Rank direct InfoDisp GetProperty candidates; assign no semantics."""
from __future__ import annotations
import argparse, collections, hashlib, json, struct
from pathlib import Path
from d3300_recon import BASE, find_refs

SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
TABLE = 0x914ED324
SINGLETON = 0x90274A7E


def analyze(data: bytes) -> dict:
    ids = {struct.unpack_from("<I", data, TABLE - BASE + i * 12)[0] for i in range(79)}
    calls = sorted(int(x["rom_address"], 16) for x in
                   find_refs(data, SINGLETON, limit=5000, mode="branches", isa="thumb"))
    found = {}
    for call in calls:
        chunk = data[call - BASE + 4:call - BASE + 52]
        for pos in range(0, 46, 2):
            halfword = struct.unpack_from("<H", chunk, pos)[0]
            prop = halfword & 0xFF
            if halfword & 0xFF00 != 0x2100 or prop not in ids:
                continue
            has_blx = any((struct.unpack_from("<H", chunk, at)[0] & 0xFF87) == 0x4780
                          for at in range(pos + 2, min(pos + 18, 46), 2))
            if has_blx:
                site = call + 4 + pos
                found[site] = {"property_id": f"0x{prop:04X}",
                               "immediate_site": f"0x{site:08X}",
                               "preceding_singleton_call": f"0x{call:08X}"}
    counts = collections.Counter(x["property_id"] for x in found.values())
    return {"sha256": hashlib.sha256(data).hexdigest(),
            "singleton_call_candidates": len(calls),
            "direct_consumer_candidates": len(found),
            "property_ids_with_candidates": len(counts),
            "counts": dict(sorted(counts.items())),
            "candidates": list(found.values()),
            "warning": "candidate syntax only; no property meaning or callable ABI is inferred"}


def verify(r: dict) -> bool:
    return (r["sha256"] == SHA256 and r["singleton_call_candidates"] == 1174
            and r["direct_consumer_candidates"] == 155
            and r["property_ids_with_candidates"] == 57
            and r["counts"].get("0x0040") == 1 and r["counts"].get("0x0054") == 2)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__); p.add_argument("firmware", type=Path); p.add_argument("--verify", action="store_true")
    a = p.parse_args(); result = analyze(a.firmware.read_bytes())
    if a.verify and not verify(result): raise SystemExit("consumer-candidate anchors changed")
    print(json.dumps(result, indent=2)); return 0


if __name__ == "__main__": raise SystemExit(main())
