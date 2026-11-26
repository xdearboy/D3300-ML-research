import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import d3300_runtime_boundaries as boundaries


FIRMWARE = Path(os.environ.get("D3300_FIRMWARE", ""))


@unittest.skipUnless(FIRMWARE.is_file(), "D3300_FIRMWARE not available")
class RuntimeBoundaryTests(unittest.TestCase):
    def test_exact_c102_boundaries(self):
        self.assertTrue(boundaries.verify(boundaries.analyze(FIRMWARE.read_bytes())))


if __name__ == "__main__":
    unittest.main()
