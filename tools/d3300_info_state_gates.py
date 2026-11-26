#!/usr/bin/env python3
"""Verify bounded UI-state use of InfoDisp properties 0x005A and 0x005C."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from d3300_info_consumer_candidates import analyze as candidates

BASE = 0x90020000
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"

def at(data: bytes, address: int, expected: str) -> bool:
    raw = bytes.fromhex(expected); off = address - BASE
    return data[off:off + len(raw)] == raw

def analyze(data: bytes) -> dict:
    inventory = candidates(data)
    checks = [
        {"address": "0x90252240", "observation": "0x005A value is bounded below 14 then enters TBB",
         "match": at(data, 0x90252240, "009900200e2926d2dfe801f0")},
        {"address": "0x9025428C", "observation": "query 0x005A, require value 13, then query 0x005C",
         "match": at(data, 0x9025428C, "20f0f7fb0446006804225a216b4606682046b0474ff07046b0420bd100980d280fd1206804225c2101ab07682046b847")},
        {"address": "0x90258654", "observation": "adjacent four-byte getter wrappers for 0x005A and 0x005C",
         "match": at(data, 0x90258654, "1cf013fa016804226b460c685a21a047009838bd38b5002000901cf006fa016804226b460c685c21a047009838bd")},
    ]
    return {"sha256": hashlib.sha256(data).hexdigest(),
            "candidate_counts": {"0x005A": inventory["counts"]["0x005A"], "0x005C": inventory["counts"]["0x005C"]},
            "checks": checks,
            "classification": "UI state gates; concrete state names unproven",
            "terminal_semantic_evidence": None}

def verify(r: dict) -> bool:
    return (r["sha256"] == SHA256 and r["candidate_counts"] == {"0x005A": 29, "0x005C": 12}
            and all(x["match"] for x in r["checks"]) and r["terminal_semantic_evidence"] is None)

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__); p.add_argument("firmware",type=Path); p.add_argument("--verify",action="store_true")
    a=p.parse_args(); r=analyze(a.firmware.read_bytes())
    if a.verify and not verify(r): raise SystemExit("InfoDisp state-gate anchors changed")
    print(json.dumps(r,indent=2)); return 0

if __name__ == "__main__": raise SystemExit(main())
