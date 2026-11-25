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
        self.assertEqual(thumb_bl_target(0x900EFAFC, self.raw(0x900EFAFC, 4)), 0x9012A73C)

    def test_draw_state_reaches_output_object_and_kernel_veneer(self):
        """Observed path calls output-object slot +0x5C then the kernel veneer at 0x90225D70."""
        self.assertEqual(self.raw(0x90129FA0, 8), bytes.fromhex("3068c26d30469047"))
        self.assertEqual(self.raw(0x90129868, 8), bytes.fromhex("c120fcf082ea0446"))

    def test_cguicdraw_has_five_initialized_state_slots(self):
        """Constructor installs the CGUICDraw vtable and iterates exactly five slots."""
        self.assertEqual(struct.unpack("<I", self.raw(0x9012AD90, 4))[0], 0x9154AD28)
        # 0x9012AD38..0x9012AD40 forms index * 19, then index * 76 bytes.
        self.assertEqual(self.raw(0x9012AD38, 12), bytes.fromhex("05eb450000eb051006eb8001"))
        self.assertEqual(self.raw(0x9012AD5A, 6), bytes.fromhex("6d1c4b70052d"))

    def test_cguicdraw_handoff_calls_state_manager(self):
        """0x9012A73C copies selected state then directly invokes 0x90129EFE."""
        self.assertEqual(thumb_bl_target(0x9012A7A4, self.raw(0x9012A7A4, 4)), 0x90129EFE)

    def test_cguicdraw_static_global_is_constructed_in_rom_startup_wrapper(self):
        """The constructor wrapper supplies the static RAM object 0x10383580."""
        self.assertEqual(struct.unpack("<I", self.raw(0x9012ADB4, 4))[0], 0x10383580)
        self.assertEqual(thumb_bl_target(0x9012ADA2, self.raw(0x9012ADA2, 4)), 0x9012AD16)

    def test_cguicdraw_public_wrapper_also_reaches_state_manager(self):
        """The direct ROM caller at 0x9012A106 supplies the static draw object."""
        self.assertEqual(struct.unpack("<I", self.raw(0x9012A51C, 4))[0], 0x10383580)
        self.assertEqual(thumb_bl_target(0x9012A11E, self.raw(0x9012A11E, 4)), 0x90129EFE)

    def test_output_object_virtual_slots_are_control_flow_only(self):
        """State manager invokes output-object slots; their implementations are dynamic."""
        self.assertEqual(self.raw(0x90129FBE, 8), bytes.fromhex("3068016a30468847"))  # +0x20
        self.assertEqual(self.raw(0x9012A012, 10), bytes.fromhex("30683a46836e30469847"))  # +0x68
        self.assertEqual(self.raw(0x9012A028, 10), bytes.fromhex("30683946026f30469047"))  # +0x70
        self.assertEqual(self.raw(0x9012A062, 10), bytes.fromhex("30680021426830469047"))  # +0x04
        self.assertEqual(self.raw(0x90129AE6, 16), bytes.fromhex("2868012102aad0f894302846984754f8"))  # +0x94

    def test_pre_output_veneer_has_only_a_dynamic_ram_target(self):
        """The pre-output veneer target is a RAM literal; no syscall name/ABI is claimed."""
        self.assertEqual(struct.unpack("<I", self.raw(0x90225EA4, 4))[0], 0xE51FF004)
        self.assertEqual(struct.unpack("<I", self.raw(0x90225EA8, 4))[0], 0x10099244)
        self.assertEqual(self.raw(0x90129F48, 4), bytes.fromhex("fbf0acef"))


if __name__ == "__main__":
    unittest.main()
