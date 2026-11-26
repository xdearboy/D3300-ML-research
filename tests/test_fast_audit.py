import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class FastAuditTest(unittest.TestCase):
    def test_cache_hit_skips_checks(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            fw = root / "fw.bin"
            fw.write_bytes(b"synthetic firmware")
            digest = hashlib.sha256(fw.read_bytes()).hexdigest()
            cache_dir = root / "cache"
            cache_dir.mkdir()
            tool_dir = Path("tools")
            fingerprint = hashlib.sha256()
            for name in ("d3300_fast_audit.py", "d3300_verify_camera_status.py",
                         "d3300_info_disp_map.py", "d3300_tkernel_veneer_audit.py",
                         "d3300_runtime_boundaries.py", "d3300_extension_seams.py",
                         "d3300_info_consumer_candidates.py",
                         "d3300_info_state_gates.py", "d3300_state_route_analyzer.py"):
                fingerprint.update(name.encode() + b"\0" + (tool_dir / name).read_bytes())
            cache = cache_dir / f"{digest}-{fingerprint.hexdigest()}.json"
            cache.write_text(json.dumps({"sha256": digest, "cached": True}))
            p = subprocess.run([
                sys.executable, "tools/d3300_fast_audit.py", str(fw), "--cache-dir", str(cache_dir)
            ], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0)
            self.assertIn("CACHE", p.stdout)


if __name__ == "__main__":
    unittest.main()
