"""Tests verifying the On-Screen Display (OSD) vector rendering pipeline,

10-byte primitive layout at 0x903111BC, clipping, and COSD class vtables.
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


class OSDRenderingTest(unittest.TestCase):
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

    def test_osd_primitive_appender_0x903111bc(self):
        raw = self.read_bytes(0x903111BC, 40)
        hw = struct.unpack("<20H", raw)

        # 0x903111BC: push {r4, r5, r6, lr} -> 0xB570
        self.assertEqual(hw[0], 0xB570, "push {r4, r5, r6, lr}")
        # 0x903111BE: ldr r0, [r2] -> 0x6810
        self.assertEqual(hw[1], 0x6810, "ldr r0, [r2] (*count_ptr)")
        # 0x903111C0: ldrh r6, [r3] -> 0x881E
        self.assertEqual(hw[2], 0x881E, "ldrh r6, [r3] (source rect X)")
        # 0x903111C2: ldr r4, [sp, #0x10] -> 0x9C04
        self.assertEqual(hw[3], 0x9C04, "ldr r4, [sp, #0x10] (attribute/color)")
        # 0x903111C4: add.w r5, r0, r0, lsl #2 (r5 = count * 5) -> 0xEB00, 0x0580
        self.assertEqual((hw[4], hw[5]), (0xEB00, 0x0580), "add.w r5, r0, r0, lsl #2")
        # 0x903111C8: adds r0, r0, #1 -> 0x1C40
        self.assertEqual(hw[6], 0x1C40, "adds r0, r0, #1 (count++)")
        # 0x903111CA: strh.w r6, [r1, r5, lsl #1] (dest[count * 10 + 0] = X) -> 0xF821, 0x6015
        self.assertEqual((hw[7], hw[8]), (0xF821, 0x6015), "strh.w r6, [r1, r5, lsl #1]")
        # 0x903111CE: add.w r1, r1, r5, lsl #1 (r1 = &dest[count * 10]) -> 0xEB01, 0x0145
        self.assertEqual((hw[9], hw[10]), (0xEB01, 0x0145), "add.w r1, r1, r5, lsl #1 (stride 10)")
        # 0x903111D2: ldrh r5, [r3, #2] -> 0x885D
        self.assertEqual(hw[11], 0x885D, "ldrh r5, [r3, #2] (source rect Y)")
        # 0x903111D4: strh r5, [r1, #2] -> 0x804D
        self.assertEqual(hw[12], 0x804D, "strh r5, [r1, #2] (dest Y)")
        # 0x903111D6: ldrh r5, [r3, #4] -> 0x889D
        self.assertEqual(hw[13], 0x889D, "ldrh r5, [r3, #4] (source rect W)")
        # 0x903111D8: strh r5, [r1, #4] -> 0x808D
        self.assertEqual(hw[14], 0x808D, "strh r5, [r1, #4] (dest W)")
        # 0x903111DA: ldrh r3, [r3, #6] -> 0x88DB
        self.assertEqual(hw[15], 0x88DB, "ldrh r3, [r3, #6] (source rect H)")
        # 0x903111DC: strh r3, [r1, #6] -> 0x80CB
        self.assertEqual(hw[16], 0x80CB, "strh r3, [r1, #6] (dest H)")
        # 0x903111DE: strb r4, [r1, #8] (dest attribute) -> 0x720C
        self.assertEqual(hw[17], 0x720C, "strb r4, [r1, #8] (dest attribute)")
        # 0x903111E0: str r0, [r2] (*count_ptr = count + 1) -> 0x6010
        self.assertEqual(hw[18], 0x6010, "str r0, [r2] (store count)")
        # 0x903111E2: pop {r4, r5, r6, pc} -> 0xBD70
        self.assertEqual(hw[19], 0xBD70, "pop {r4, r5, r6, pc}")

    def test_cosd_class_vtables_and_rtti(self):
        classes = [
            ("COSDDControl", 0x91517260, 0x91500F70, "12COSDDControl"),
            ("COSDCRectInfo", 0x91518294, 0x91501320, "13COSDCRectInfo"),
            ("COSDCColorPalette", 0x9151FBB0, 0x915029F8, "17COSDCColorPalette"),
            ("COSDDCreateAudioMeter", 0x9152D420, 0x91504AAC, "21COSDDCreateAudioMeter"),
            ("COSDDCreateAutofocus", 0x915292B4, 0x915041D0, "20COSDDCreateAutofocus"),
        ]
        for name, vtable, rtti, symbol in classes:
            rtti_ptr = self.read_words(vtable - 4, 1)[0]
            self.assertEqual(rtti_ptr, rtti, f"{name} RTTI pointer mismatch")
            self.assertEqual(self.read_rtti_name(rtti_ptr), symbol, f"{name} symbol mismatch")

    def test_cosdd_control_submit_rect_list_stride(self):
        # COSDDControl slot +0x04 at 0x902EDE22
        # Lines 0x902EDE4E..0x902EDE54:
        # add.w r0, r5, r5, lsl #1 (count * 3)
        # add.w r1, r6, r0, lsl #2 (rect_array + count * 12) -> stride 12 bytes
        raw = self.read_bytes(0x902EDE4E, 8)
        hw = struct.unpack("<4H", raw)
        self.assertEqual((hw[0], hw[1]), (0xEB05, 0x0045), "add.w r0, r5, r5, lsl #1 (count * 3)")
        self.assertEqual((hw[2], hw[3]), (0xEB06, 0x0180), "add.w r1, r6, r0, lsl #2 (stride 12)")

    def test_osd_byte_0x09_unwritten_semantics(self):
        """Byte +0x09 is not written by 0x903111BC; consumer semantics remain UNPROVEN."""
        raw = self.read_bytes(0x903111DE, 6)
        hw = struct.unpack("<3H", raw)
        # 0x903111DE: strb r4, [r1, #8] -> 0x720C
        self.assertEqual(hw[0], 0x720C, "strb r4, [r1, #8]")
        # 0x903111E0: str r0, [r2] -> 0x6010 (writes count, no write to [r1, #9])
        self.assertEqual(hw[1], 0x6010, "str r0, [r2]")
        # 0x903111E2: pop {r4, r5, r6, pc} -> 0xBD70
        self.assertEqual(hw[2], 0xBD70, "pop {r4, r5, r6, pc}")


if __name__ == "__main__":
    unittest.main()
