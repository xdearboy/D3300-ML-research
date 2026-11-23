"""Tests for derived RAM reference tracking using synthetic disassembly."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_derived_refs import find_derived_refs


class DerivedRefsTest(unittest.TestCase):
    def test_tracks_literal_add_and_str(self):
        lines = [
            "9013b9aa: 483f  ldr  r0, [pc, #0xfc]  @ 0x9013baa8 <.text+0x11baa8>",
            "9013b9ac: 30b4  adds r0, #0xb4",
            "9013b9ae: 6601  str  r1, [r0, #0x60]",
            "9013b9b0: 4770  bx   lr",
        ]
        literals = {0x9013BAA8: 0x10491468}
        matches = list(find_derived_refs(lines, literals=literals))
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["address"], "0x9013B9AE")
        self.assertEqual(matches[0]["access"], "store")
        self.assertEqual(matches[0]["base_address"], "0x10491468")
        self.assertEqual(matches[0]["offset"], 0x114)  # 0xB4 + 0x60
        self.assertEqual(matches[0]["derived_address"], "0x1049157C")

    def test_tracks_mov_and_ldr(self):
        lines = [
            "90001000: 4802  ldr  r0, [pc, #8]  @ 0x1003B5B4",
            "90001002: 4601  mov  r1, r0",
            "90001004: 3120  adds r1, #0x20",
            "90001006: 684a  ldr  r2, [r1, #0x4]",
        ]
        matches = list(find_derived_refs(lines))
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["address"], "0x90001006")
        self.assertEqual(matches[0]["access"], "load")
        self.assertEqual(matches[0]["derived_address"], "0x1003B5D8")  # 0x1003B5B4 + 0x24

    def test_target_filter(self):
        lines = [
            "90002000: 4802  ldr  r0, [pc, #8]  @ 0x1012B908",
            "90002002: 3010  adds r0, #0x10",
            "90002004: 6801  ldr  r1, [r0]",
            "90002006: 3010  adds r0, #0x10",
            "90002008: 6802  ldr  r2, [r0]",
        ]
        # Match only first target
        target = 0x1012B918
        matches = list(find_derived_refs(lines, target=target))
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["address"], "0x90002004")
        self.assertEqual(matches[0]["derived_address"], "0x1012B918")

        # Non-matching target
        no_matches = list(find_derived_refs(lines, target=0x1012FFFF))
        self.assertEqual(len(no_matches), 0)

    def test_rejects_clobbered_register_or_branch(self):
        lines = [
            "90003000: 4802  ldr  r0, [pc, #8]  @ 0x10000000",
            "90003002: 2000  movs r0, #0",
            "90003004: 6801  ldr  r1, [r0, #4]",
            "90003006: 4802  ldr  r2, [pc, #8]  @ 0x10000000",
            "90003008: 4770  bx   lr",
            "9000300a: 6853  ldr  r3, [r2, #4]",
        ]
        matches = list(find_derived_refs(lines))
        self.assertEqual(len(matches), 0)


if __name__ == "__main__":
    unittest.main()
