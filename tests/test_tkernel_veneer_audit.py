import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import d3300_tkernel_veneer_audit as audit


FIRMWARE = Path(os.environ.get("D3300_FIRMWARE", ""))


@unittest.skipUnless(FIRMWARE.is_file(), "D3300_FIRMWARE not available")
class TKernelVeneerAuditTests(unittest.TestCase):
    def test_exact_veneer_shape_without_api_name_claims(self):
        result = audit.analyze(FIRMWARE.read_bytes())
        self.assertEqual(result["image_sha256"], audit.EXPECTED_SHA256)
        self.assertEqual(result["veneer_count"], 471)
        self.assertEqual(result["first_veneer"], "0x90225004")
        self.assertEqual(result["last_veneer"], "0x90226060")
        self.assertEqual(result["target_classes"], {
            "ram": 467, "rom": 4, "outside_expected_ranges": 0,
        })
        self.assertEqual(
            [item["target"] for item in result["rom_target_veneers"]],
            ["0x90025A49", "0x902EB6A1", "0x9030D2BD", "0x90274B6D"],
        )


if __name__ == "__main__":
    unittest.main()
