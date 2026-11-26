"""Tests for D3300 menu class hierarchy and ROM control-flow anchors.

The RAM observations here do not establish a safe external read or hook ABI.
"""

from __future__ import annotations

import hashlib
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


class MenuArchitectureTest(unittest.TestCase):
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

    def test_firmware_sha256(self):
        data = firmware()
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

    def test_cmenu_controller_vtable_and_rtti(self):
        vtable = 0x9151B62C
        rtti_ptr = self.read_words(vtable - 4, 1)[0]
        self.assertEqual(rtti_ptr, 0x91501D8C)
        self.assertEqual(self.read_rtti_name(rtti_ptr), "15CMENUController")

        # Key slots
        slot3c = self.read_words(vtable + 0x3C, 1)[0]
        self.assertEqual(slot3c, 0x9002A5F1, "Slot +0x3C points to 0x9002A5F0")

        slot44 = self.read_words(vtable + 0x44, 1)[0]
        self.assertEqual(slot44, 0x9002A62D, "Slot +0x44 points to 0x9002A62C")

        slot60 = self.read_words(vtable + 0x60, 1)[0]
        self.assertEqual(slot60, 0x90032AF3, "Slot +0x60 points to 0x90032AF2")

        slot64 = self.read_words(vtable + 0x64, 1)[0]
        self.assertEqual(slot64, 0x90032CA5, "Slot +0x64 points to 0x90032CA4")

    def test_active_screen_getter_0x90031d2a_and_evaluator_0x9003ff50(self):
        raw_caller = self.read_bytes(0x90031D2A, 6)
        hw = struct.unpack("<3H", raw_caller)
        self.assertEqual(hw[0], 0x6980, "ldr r0, [r0, #0x18]")
        self.assertEqual(hw[1], 0xF00E, "b.w 0x9003FF50")
        self.assertEqual(hw[2], 0xB910)

        raw_eval = self.read_bytes(0x9003FF50, 20)
        hw_eval = struct.unpack("<10H", raw_eval)
        self.assertEqual(hw_eval[0], 0xB538, "push {r3..r5, lr}")
        self.assertEqual(hw_eval[1], 0x2100, "movs r1, #0")
        self.assertEqual(hw_eval[2], 0x300C, "adds r0, #0x0C")

        # 0x9003FF7C: ldr r0, [r0, #8] (extract view from CGUICEntry)
        raw_extract = self.read_bytes(0x9003FF7C, 4)
        hw_extract = struct.unpack("<2H", raw_extract)
        self.assertEqual(hw_extract[0], 0x6880, "ldr r0, [r0, #8] (entry->view)")

    def test_cguic_entry_and_view_vtables(self):
        entry_vt = 0x91515E84
        entry_rtti = self.read_words(entry_vt - 4, 1)[0]
        self.assertEqual(entry_rtti, 0x91500AC8)
        self.assertEqual(self.read_rtti_name(entry_rtti), "10CGUICEntry")

        view_vt = 0x9154ADC8
        view_rtti = self.read_words(view_vt - 4, 1)[0]
        self.assertEqual(view_rtti, 0x915085FC)
        self.assertEqual(self.read_rtti_name(view_rtti), "9CGUICView")

    def test_cmenu_top_menu_view_vtable_and_rtti(self):
        vtable = 0x9151D874
        rtti_ptr = self.read_words(vtable - 4, 1)[0]
        self.assertEqual(rtti_ptr, 0x91502354)
        self.assertEqual(self.read_rtti_name(rtti_ptr), "16CMENUTopMenuView")

        # Base classes in RTTI
        base_count = self.read_words(rtti_ptr + 12, 1)[0]
        self.assertEqual(base_count, 2, "CMENUTopMenuView has 2 base classes")

        base1_ptr = self.read_words(rtti_ptr + 16, 1)[0]
        self.assertEqual(self.read_rtti_name(base1_ptr), "13CMENUViewBase")

        base2_ptr = self.read_words(rtti_ptr + 24, 1)[0]
        self.assertEqual(self.read_rtti_name(base2_ptr), "27IGUICSettingValueAccessorIF")

        # Event dispatch slots
        slot10 = self.read_words(vtable + 0x10, 1)[0]
        self.assertEqual(slot10, 0x9025A85F, "Slot +0x10 is 0x9025A85E")

        slot40 = self.read_words(vtable + 0x40, 1)[0]
        self.assertEqual(slot40, 0x9025AD55, "Slot +0x40 is 0x9025AD54")

        slot44 = self.read_words(vtable + 0x44, 1)[0]
        self.assertEqual(slot44, 0x90254DD5, "Slot +0x44 is 0x90254DD4")

    def test_screen_event_filter_tbb_0x9025a86e(self):
        # Range check: cmp r1, #0x64; bhs 0x9025A952
        hw_check = self.read_halfwords(0x9025A860, 5)
        self.assertEqual(hw_check[0], 0x2964, "cmp r1, #0x64 (100 entries)")
        self.assertEqual(hw_check[4], 0xD273, "bhs 0x9025A952")

        table = self.read_bytes(0x9025A86E, 100)
        # All button codes 1..28 must branch to 0x9025A8D2
        target_byte = (0x9025A8D2 - 0x9025A86E) // 2
        for code in range(1, 29):
            self.assertEqual(
                table[code],
                target_byte,
                f"Logical code {code} must route to 0x9025A8D2",
            )

    def test_view_action_router_tbb_0x90254e1c(self):
        # Range check: cmp r5, #0x1D; bhs 0x90254E70
        hw_check = self.read_halfwords(0x90254E14, 2)
        self.assertEqual(hw_check[0], 0x2D1D, "cmp r5, #0x1D (29 entries)")
        self.assertEqual(hw_check[1], 0xD22B, "bhs 0x90254E70")

        table = self.read_bytes(0x90254E1C, 29)
        base = 0x90254E1C

        # Code 1 (Logical ID 0x01) -> 0x90254E7E -> slot +0x12C [inferred: Confirm / Action]
        target_code1 = base + table[1] * 2
        self.assertEqual(target_code1, 0x90254E7E)
        slot12c_ldr = self.read_halfwords(target_code1 + 2, 2)
        self.assertEqual((slot12c_ldr[0], slot12c_ldr[1]), (0xF8D0, 0x212C), "ldr.w r2, [r0, #0x12c]")

        # Code 5 (Logical ID 0x05) -> 0x90254E86 -> slot +0x130 [inferred: Nav Right]
        target_code5 = base + table[5] * 2
        self.assertEqual(target_code5, 0x90254E86)
        slot130_ldr = self.read_halfwords(target_code5 + 2, 2)
        self.assertEqual((slot130_ldr[0], slot130_ldr[1]), (0xF8D0, 0x2130), "ldr.w r2, [r0, #0x130]")

        # Code 27 (Logical ID 0x1B) -> 0x90254F1E -> slot +0x17C [inferred: Menu Toggle / Back]
        target_code27 = base + table[27] * 2
        self.assertEqual(target_code27, 0x90254F1E)
        slot17c_ldr = self.read_halfwords(target_code27 + 2, 2)
        self.assertEqual((slot17c_ldr[0], slot17c_ldr[1]), (0xF8D0, 0x217C), "ldr.w r2, [r0, #0x17c]")

    def test_active_view_pointer_writer_0x9002a6ac(self):
        raw = self.read_bytes(0x9002A6A8, 6)
        hw = struct.unpack("<3H", raw)
        self.assertEqual(hw[0], 0x499F, "ldr r1, [literal pool] (0x10123844)")
        self.assertEqual(hw[1], 0x68A8, "ldr r0, [r5, #8]")
        self.assertEqual(hw[2], 0x6008, "str r0, [r1]")

        literal = self.read_words(0x9002A928, 1)[0]
        self.assertEqual(literal, 0x10123844, "Active view pointer stored at 0x10123844")


if __name__ == "__main__":
    unittest.main()
