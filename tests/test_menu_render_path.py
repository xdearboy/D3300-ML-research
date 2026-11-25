"""Static anchors for the stock-menu-to-graphics message path in D3300 C 1.02.

This proves ROM control-flow edges only.  It does not claim a pixel format,
hardware scan-out, a writable buffer, or a camera-safe rendering ABI.
"""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import struct
import unittest

BASE = 0x90020000
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def image() -> bytes:
    path = os.environ.get("D3300_FIRMWARE")
    if not path or not Path(path).is_file():
        raise unittest.SkipTest("D3300_FIRMWARE is required")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise AssertionError("unexpected firmware SHA-256")
    return data


def thumb_bl_target(address: int, raw: bytes) -> int:
    hw1, hw2 = struct.unpack("<HH", raw)
    if (hw1 & 0xF800) != 0xF000 or (hw2 & 0xD000) != 0xD000:
        raise AssertionError(f"not a T32 branch at 0x{address:08X}")
    s = (hw1 >> 10) & 1
    i1 = (~(((hw2 >> 13) & 1) ^ s)) & 1
    i2 = (~(((hw2 >> 11) & 1) ^ s)) & 1
    displacement = ((s << 24) | (i1 << 23) | (i2 << 22) |
                    ((hw1 & 0x3FF) << 12) | ((hw2 & 0x7FF) << 1))
    if s:
        displacement -= 1 << 25
    return address + 4 + displacement


class MenuRenderPathTest(unittest.TestCase):
    def raw(self, address: int, size: int) -> bytes:
        data = image()
        return data[address - BASE:address - BASE + size]

    def test_menu_view_uses_vtable_slot_0x140(self):
        """CMENUTopMenuView setup calls its observed slot +0x140."""
        self.assertEqual(self.raw(0x900767CE, 10), bytes.fromhex("2068d0f8402120469047"))

    def test_cguic_task_dispatches_two_view_slots(self):
        """CGUICTask branches dispatch its view object through slots +0x10/+0x14."""
        raw = self.raw(0x900EE49C, 36)
        self.assertEqual(raw[:12], bytes.fromhex("b26803681b690ae0002da4d0"))
        self.assertEqual(raw[24:32], bytes.fromhex("b26803685b699847"))

    def test_view_path_calls_draw_state_manager(self):
        """The view continuation directly calls the draw-state manager at 0x9012A73C."""
        self.assertEqual(thumb_bl_target(0x900EF8D4, self.raw(0x900EF8D4, 4)), 0x9012A73C)

    def test_draw_state_reaches_output_object_and_kernel_veneer(self):
        """Observed path calls output-object slot +0x5C then the kernel veneer at 0x90225D70."""
        self.assertEqual(self.raw(0x90129FA0, 8), bytes.fromhex("3068c26d30469047"))
        self.assertEqual(self.raw(0x90129868, 8), bytes.fromhex("c120fcf082ea0446"))


if __name__ == "__main__":
    unittest.main()
