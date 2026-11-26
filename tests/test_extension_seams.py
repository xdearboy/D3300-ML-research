import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import d3300_extension_seams as seams

FIRMWARE = Path(os.environ.get("D3300_FIRMWARE", ""))


@unittest.skipUnless(FIRMWARE.is_file(), "D3300_FIRMWARE not available")
class ExtensionSeamTests(unittest.TestCase):
    def test_exact_c102_classification(self):
        result = seams.analyze(FIRMWARE.read_bytes())
        self.assertTrue(seams.verify(result))
        self.assertEqual(result["safe_hook_count"], 0)
        self.assertNotIn("safe", {item["classification"] for item in result["seams"]})


if __name__ == "__main__":
    unittest.main()
