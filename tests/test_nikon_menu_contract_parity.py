"""Ensure the host-only menu contract is mechanically identical to the ROM TBB."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import struct
import unittest

BASE = 0x90020000
TBB_BASE = 0x90252DAA
ENTRY_COUNT = 59
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def firmware() -> bytes:
    path = os.environ.get("D3300_FIRMWARE")
    if not path or not Path(path).is_file():
        raise unittest.SkipTest("D3300_FIRMWARE is required")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise AssertionError("unexpected firmware SHA-256")
    return data


class NikonMenuContractParityTest(unittest.TestCase):
    def test_c_table_equals_rom_tbb_translation(self):
        """Each C entry equals the MOVS r1 immediate at its ROM TBB target."""
        source = (Path(__file__).resolve().parents[1] /
                  "core" / "dml_nikon_menu_contract.c").read_text(encoding="utf-8")
        body = re.search(r"k_raw_to_logical\[59\]\s*=\s*\{(.*?)\};", source, re.S)
        self.assertIsNotNone(body)
        host_values = [int(value) for value in re.findall(r"\b(\d+)u", body.group(1))]
        self.assertEqual(len(host_values), ENTRY_COUNT)

        data = firmware()
        rom_values = []
        for raw_code in range(ENTRY_COUNT):
            offset = data[TBB_BASE - BASE + raw_code]
            target = TBB_BASE + offset * 2
            halfword = struct.unpack_from("<H", data, target - BASE)[0]
            # All mapped target blocks begin `movs r1, #logical_id`; the
            # default target is `bx lr`, so no logical ID is produced.
            if (halfword & 0xFF00) == 0x2100:
                rom_values.append(halfword & 0xFF)
            else:
                self.assertEqual(halfword, 0x4770, f"unexpected TBB default at raw {raw_code}")
                rom_values.append(0)

        self.assertEqual(host_values, rom_values)
        # These are raw event codes, deliberately not physical MENU/OK labels.
        self.assertEqual((rom_values[0x0B], rom_values[0x0C]), (0x1B, 0x01))


if __name__ == "__main__":
    unittest.main()
