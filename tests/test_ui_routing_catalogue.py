"""Tests verifying the complete UI event routing catalogue, TBB branch tables,

vtable slot mappings, and CJAPFButtonEventParam lifecycle on Nikon D3300 C 1.02.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import struct
import unittest

BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def firmware() -> bytes:
    path = os.environ.get("D3300_FIRMWARE")
    if not path:
        default_candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if default_candidate.is_file():
            path = str(default_candidate)
    if not path or not Path(path).is_file():
        raise unittest.SkipTest(f"Firmware binary not found at {path}")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise AssertionError("unexpected firmware SHA-256")
    return data


class UIRoutingCatalogueTest(unittest.TestCase):
    def read_bytes(self, addr: int, size: int) -> bytes:
        data = firmware()
        off = addr - BASE
        return data[off : off + size]

    def read_halfwords(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 2)
        return struct.unpack(f"<{count}H", raw)

    def read_words(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 4)
        return struct.unpack(f"<{count}I", raw)

    def read_rtti_name(self, rtti_addr: int) -> str:
        name_ptr = self.read_words(rtti_addr + 4, 1)[0]
        raw = self.read_bytes(name_ptr, 64).split(b"\x00")[0]
        return raw.decode("ascii", errors="replace")

    def test_01_firmware_sha256(self):
        """Firmware image must match verified b860_102 SHA-256."""
        data = firmware()
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

    def test_02_tbb1_bounds_and_structure(self):
        """TBB Table 1 at 0x90252DAA must have 59 entries with bounds check cmp r1, #0x3B."""
        # Range check: cmp r1, #0x3B (0x293B) at 0x90252D9E
        check_hw = self.read_halfwords(0x90252D9E, 1)[0]
        self.assertEqual(check_hw, 0x293B, "Must check cmp r1, #0x3B")

        # Table branch: tbb [pc, r1] (0xE8DF 0xF001) at 0x90252DA6
        tbb_hw = self.read_halfwords(0x90252DA6, 2)
        self.assertEqual(tbb_hw, (0xE8DF, 0xF001), "Must be tbb [pc, r1]")

        table = self.read_bytes(0x90252DAA, 59)
        self.assertEqual(len(table), 59)

        # Raw 0x0C (12) -> Logical 1 (0x01)
        target_0c = 0x90252DAA + table[0x0C] * 2
        ins_0c = self.read_halfwords(target_0c, 1)[0]
        self.assertEqual(ins_0c, 0x2101, "Raw 0x0C must load logical ID 1 (movs r1, #1)")

        # Raw 0x0B (11) -> Logical 27 (0x1B)
        target_0b = 0x90252DAA + table[0x0B] * 2
        ins_0b = self.read_halfwords(target_0b, 1)[0]
        self.assertEqual(ins_0b, 0x211B, "Raw 0x0B must load logical ID 27 (movs r1, #0x1B)")

        # Raw 0x10 (16) -> Logical 5 (0x05)
        target_10 = 0x90252DAA + table[0x10] * 2
        ins_10 = self.read_halfwords(target_10, 1)[0]
        self.assertEqual(ins_10, 0x2105, "Raw 0x10 must load logical ID 5 (movs r1, #5)")

    def test_03_tbb2_bounds_and_button_dispatcher(self):
        """TBB Table 2 at 0x9025A86E must route all 40 logical button IDs to 0x9025A8D2."""
        # Range check: cmp r1, #0x64 (0x2964) at 0x9025A860
        check_hw = self.read_halfwords(0x9025A860, 1)[0]
        self.assertEqual(check_hw, 0x2964, "Must check cmp r1, #0x64")

        table = self.read_bytes(0x9025A86E, 100)
        self.assertEqual(len(table), 100)

        expected_target = 0x9025A8D2
        expected_byte = (expected_target - 0x9025A86E) // 2

        # Codes 1..28 and 33..44
        expected_button_ids = list(range(1, 29)) + list(range(33, 45))
        for lid in expected_button_ids:
            self.assertEqual(
                table[lid],
                expected_byte,
                f"Logical ID {lid} must route to primary button handler 0x9025A8D2",
            )

        # Dispatcher at 0x9025A8D2 loads slot +0xC4 then slot +0x40
        disp_hw = self.read_halfwords(0x9025A8DA, 4)
        self.assertEqual((disp_hw[0], disp_hw[1]), (0xF8D0, 0x20C4), "Must load slot +0xC4")
        self.assertEqual(disp_hw[3], 0x4790, "Must call blx r2 (PreEventHook)")

    def test_04_tbb3_bounds_and_slot_router(self):
        """TBB Table 3 at 0x90254E1C must route logical IDs 1..28 to vtable slots +0x120..+0x184."""
        # Range check: cmp r5, #0x1D (0x2D1D) at 0x90254E14
        check_hw = self.read_halfwords(0x90254E14, 1)[0]
        self.assertEqual(check_hw, 0x2D1D, "Must check cmp r5, #0x1D")

        table = self.read_bytes(0x90254E1C, 29)
        self.assertEqual(len(table), 29)

        # Logical 1 -> slot +0x12C
        tgt1 = 0x90254E1C + table[1] * 2
        hw1 = self.read_halfwords(tgt1 + 2, 2)
        self.assertEqual((hw1[0], hw1[1]), (0xF8D0, 0x212C), "Logical 1 must load slot +0x12C")

        # Logical 5 -> slot +0x130
        tgt5 = 0x90254E1C + table[5] * 2
        hw5 = self.read_halfwords(tgt5 + 2, 2)
        self.assertEqual((hw5[0], hw5[1]), (0xF8D0, 0x2130), "Logical 5 must load slot +0x130")

        # Logical 27 -> slot +0x17C
        tgt27 = 0x90254E1C + table[27] * 2
        hw27 = self.read_halfwords(tgt27 + 2, 2)
        self.assertEqual((hw27[0], hw27[1]), (0xF8D0, 0x217C), "Logical 27 must load slot +0x17C")

        # Logical 0, 23, 24 -> target 0x90254E70 (returns 1 without slot call)
        for bypass_id in (0, 23, 24):
            tgt_bypass = 0x90254E1C + table[bypass_id] * 2
            self.assertEqual(tgt_bypass, 0x90254E70, f"Logical {bypass_id} must branch to 0x90254E70")

    def test_05_cmenu_top_menu_view_action_handlers(self):
        """CMENUTopMenuView vtable at 0x9151D874 must map slots +0x120..+0x184 to valid ROM functions."""
        vtable = 0x9151D874
        for slot in range(0x120, 0x188, 4):
            ptr = self.read_words(vtable + slot, 1)[0]
            self.assertEqual(ptr & 1, 1, f"Slot +0x{slot:X} must be Thumb address")
            target = ptr & ~1
            self.assertGreaterEqual(target, 0x90020000, f"Slot +0x{slot:X} target in ROM")
            self.assertLess(target, 0x91600000, f"Slot +0x{slot:X} target in ROM")

    def test_06_event_param_vtable_and_post_dispatch_virtual_call(self):
        """Only the class vtable and a post-dispatch indirect call are statically proven."""
        # RTTI class name
        rtti_addr = 0x91504930
        self.assertEqual(self.read_rtti_name(rtti_addr), "21CJAPFButtonEventParam")

        # VTable
        vtable = 0x9152BD0C
        rtti_ptr = self.read_words(vtable - 4, 1)[0]
        self.assertEqual(rtti_ptr, rtti_addr, "Vtable must point to RTTI descriptor")

        # A post-dispatch indirect virtual call: target/object identity is not asserted here.
        raw_dtor_call = self.read_bytes(0x90032C80, 8)
        hw = struct.unpack("<4H", raw_dtor_call)
        self.assertEqual(hw[0], 0x6820, "ldr r0, [r4] (load vtable)")
        self.assertEqual(hw[1], 0x6801, "ldr r1, [r0] (load vtable slot +0x00)")
        self.assertEqual(hw[2], 0x4620, "mov r0, r4 (this pointer)")
        self.assertEqual(hw[3], 0x4788, "blx r1 (indirect virtual call)")

    def test_08_event_param_factory_control_flow(self):
        """Factory returns zero on failure and directly invokes the initializer on success."""
        self.assertEqual(
            self.read_bytes(0x9012B82A, 14),
            bytes.fromhex("10b50020c4f7e5fc04000ed02068"),
        )
        self.assertEqual(self.read_bytes(0x9012B842, 4), bytes.fromhex("fff7e5ff"))
        self.assertEqual(self.read_bytes(0x90032C18, 8), bytes.fromhex("f8f007fe04003bd0"))

    def test_07_json_catalogue_consistency(self):
        """JSON catalogue must exist and contain verified complete structures."""
        json_path = Path(__file__).resolve().parents[1] / "reports" / "verified-ui-routing-catalogue.json"
        self.assertTrue(json_path.is_file(), "Catalogue JSON must exist")
        data = json.loads(json_path.read_text(encoding="utf-8"))

        self.assertEqual(data["tbb1_logical_translation"]["entry_count"], 59)
        self.assertEqual(data["tbb2_screen_event_filter"]["entry_count"], 100)
        self.assertEqual(data["tbb3_view_action_router"]["entry_count"], 29)
        self.assertEqual(len(data["tbb2_screen_event_filter"]["handled_button_logical_ids"]), 40)
        self.assertEqual(data["event_param_lifecycle"]["class_name"], "CJAPFButtonEventParam")
        self.assertEqual(data["event_param_lifecycle"]["async_safety"], "UNPROVEN")


if __name__ == "__main__":
    unittest.main()
