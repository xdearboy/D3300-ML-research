import os, sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import d3300_info_consumer_candidates as scan

FW = Path(os.environ.get("D3300_FIRMWARE", ""))

@unittest.skipUnless(FW.is_file(), "D3300_FIRMWARE not available")
class CandidateTests(unittest.TestCase):
    def test_exact_candidate_inventory(self):
        result = scan.analyze(FW.read_bytes())
        self.assertTrue(scan.verify(result))
        self.assertIn("no property meaning", result["warning"])

if __name__ == "__main__": unittest.main()
