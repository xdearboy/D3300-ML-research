#!/usr/bin/env python3
"""Expand the verified 0x005A TBB and deduplicate its route targets."""
from __future__ import annotations
import argparse, hashlib, json, struct
from pathlib import Path

BASE=0x90020000
SHA256="c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
TABLE=0x9025224C
COUNT=14

def raw(data,address,size): return data[address-BASE:address-BASE+size]

def analyze(data:bytes)->dict:
    entries=list(raw(data,TABLE,COUNT))
    routes=[TABLE+2*x for x in entries]
    clusters=[]
    for target in sorted(set(routes)):
        states=[i for i,value in enumerate(routes) if value==target]
        # Both verified targets contain a Thumb-2 MOVW r1,#imm at +8.
        hw1,hw2=struct.unpack("<HH",raw(data,target+8,4))
        imm4=hw1&0xF; i=(hw1>>10)&1; imm3=(hw2>>12)&7; imm8=hw2&0xFF
        immediate=(imm4<<12)|(i<<11)|(imm3<<8)|imm8
        clusters.append({"target":f"0x{target:08X}","states":states,
                         "next_property_id":f"0x{immediate:04X}",
                         "movw_encoding":f"{hw1:04x}{hw2:04x}"})
    return {"sha256":hashlib.sha256(data).hexdigest(),"gate_property":"0x005A",
            "table":f"0x{TABLE:08X}","state_count":COUNT,
            "route_count":len(set(routes)),"routes":clusters,
            "semantic_warning":"route structure is proven; state and property names remain unknown"}

def verify(r:dict)->bool:
    return (r["sha256"]==SHA256 and r["state_count"]==14 and r["route_count"]==2
            and r["routes"][0]["states"]==[0,1,2,3]
            and r["routes"][0]["next_property_id"]=="0x031E"
            and r["routes"][1]["states"]==list(range(4,14))
            and r["routes"][1]["next_property_id"]=="0x031F")

def main()->int:
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("firmware",type=Path);p.add_argument("--verify",action="store_true")
    a=p.parse_args();r=analyze(a.firmware.read_bytes())
    if a.verify and not verify(r):raise SystemExit("state-route anchors changed")
    print(json.dumps(r,indent=2));return 0

if __name__=="__main__":raise SystemExit(main())
