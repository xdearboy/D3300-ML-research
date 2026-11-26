#!/usr/bin/env python3
"""Run cheap, deterministic D3300 static checks with SHA-keyed caching."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("firmware", type=Path)
    ap.add_argument("--cache-dir", type=Path, default=Path(".audit-cache"))
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()
    fw = args.firmware.resolve()
    digest = sha256(fw)
    args.cache_dir.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parent
    check_files = [root / name for name in (
        "d3300_fast_audit.py", "d3300_verify_camera_status.py",
        "d3300_info_disp_map.py", "d3300_tkernel_veneer_audit.py",
        "d3300_runtime_boundaries.py", "d3300_extension_seams.py",
        "d3300_info_consumer_candidates.py",
        "d3300_info_state_gates.py",
        "d3300_state_route_analyzer.py",
    )]
    checks_digest = hashlib.sha256()
    for path in check_files:
        checks_digest.update(path.name.encode() + b"\0" + path.read_bytes())
    fingerprint = checks_digest.hexdigest()
    cache = args.cache_dir / f"{digest}-{fingerprint}.json"
    if cache.exists() and not args.force:
        print(f"CACHE {digest} {cache}")
        print(cache.read_text())
        return 0

    checks = [
        [sys.executable, str(root / "d3300_verify_camera_status.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_info_disp_map.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_tkernel_veneer_audit.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_runtime_boundaries.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_extension_seams.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_info_consumer_candidates.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_info_state_gates.py"), str(fw), "--verify"],
        [sys.executable, str(root / "d3300_state_route_analyzer.py"), str(fw), "--verify"],
    ]
    results = []
    for cmd in checks:
        p = subprocess.run(cmd, text=True, capture_output=True)
        results.append({"command": cmd, "returncode": p.returncode, "stdout": p.stdout, "stderr": p.stderr})
        if p.returncode:
            print(p.stdout, end="")
            print(p.stderr, end="", file=sys.stderr)
            return p.returncode
    payload = {"firmware": str(fw), "sha256": digest, "checks_fingerprint": fingerprint, "checks": results}
    cache.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"PASS {digest} {cache}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
