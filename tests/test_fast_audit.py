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
            cache = cache_dir / f"{digest}.json"
            cache.write_text(json.dumps({"sha256": digest, "cached": True}))
            p = subprocess.run([
                sys.executable, "tools/d3300_fast_audit.py", str(fw), "--cache-dir", str(cache_dir)
            ], capture_output=True, text=True)
            self.assertEqual(p.returncode, 0)
            self.assertIn("CACHE", p.stdout)


if __name__ == "__main__":
    unittest.main()
