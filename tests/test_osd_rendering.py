"""Tests verifying the On-Screen Display (OSD) vector rendering pipeline,

10-byte primitive layout at 0x903111BC, clipping, COSD class vtables,
and hardware plane DMA boundary on Nikon D3300 C 1.02.
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

    def test_01_firmware_sha256(self):
        """Firmware image must match verified b860_102 SHA-256."""
        data = firmware()
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

    def test_02_osd_primitive_appender_0x903111bc(self):
        """0x903111BC must append 10-byte vector records with stride count * 5 << 1."""
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

    def test_03_cosd_class_vtables_and_rtti(self):
        """COSD family classes must have verified RTTI and vtables."""
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

    def test_04_cosdd_control_submit_rect_list_stride_and_plane_bound(self):
        """SubmitRectList at 0x902EDE22 must use 12-byte stride and enforce plane < 7."""
        # Stride calculation:
        raw_stride = self.read_bytes(0x902EDE4E, 8)
        hw_stride = struct.unpack("<4H", raw_stride)
        self.assertEqual((hw_stride[0], hw_stride[1]), (0xEB05, 0x0045), "add.w r0, r5, r5, lsl #1 (count * 3)")
        self.assertEqual((hw_stride[2], hw_stride[3]), (0xEB06, 0x0180), "add.w r1, r6, r0, lsl #2 (stride 12)")

        # Plane bound check at 0x902EDE58: cmp r0, #7; bhs 0x902EDE94
        raw_bound = self.read_bytes(0x902EDE56, 6)
        hw_bound = struct.unpack("<3H", raw_bound)
        self.assertEqual(hw_bound[0], 0x7A48, "ldrb r0, [r1, #9] (load plane_id)")
        self.assertEqual(hw_bound[1], 0x2807, "cmp r0, #7 (max 7 planes)")
        self.assertEqual(hw_bound[2], 0xD21B, "bhs 0x902ede94 (reject out-of-bounds plane)")

    def test_05_clipping_routine_0x9030c9d0(self):
        """0x9030C9D0 must load rect coordinates and clip against viewport boundaries."""
        raw = self.read_bytes(0x9030C9D0, 16)
        hw = struct.unpack("<8H", raw)
        self.assertEqual(hw[0], 0xB5F0, "push {r4..r7, lr}")
        self.assertEqual((hw[1], hw[2]), (0xF9B1, 0x4000), "ldrsh.w r4, [r1, #0] (load X)")
        self.assertEqual((hw[3], hw[4]), (0xE9DD, 0x5705), "ldrd r5, r7, [sp, #20] (load clip_w, clip_h)")
        self.assertEqual(hw[5], 0x4294, "cmp r4, r2 (compare X with clip_x)")

    def test_06_flush_display_planes_calls_dma_boundary(self):
        """FlushDisplayPlanes at 0x902EDE9A must call DMA submit helper 0x902BC5F4."""
        # 0x902EDF2C: bl 0x902BC5F4
        raw_bl = self.read_bytes(0x902EDF2C, 4)
        hw = struct.unpack("<2H", raw_bl)
        self.assertEqual((hw[0], hw[1]), (0xF7CE, 0xFB62), "bl 0x902BC5F4 (DMA submit boundary)")

    def test_07_cosdc_color_palette_slot_18_getter(self):
        """COSDCColorPalette slot +0x18 at 0x902AFF78 must return [this + 8] LUT pointer."""
        vtable = 0x9151FBB0
        slot18 = self.read_words(vtable + 0x18, 1)[0]
        self.assertEqual(slot18, 0x902AFF79, "Slot +0x18 must point to 0x902AFF78")

        raw_fn = self.read_bytes(0x902AFF78, 4)
        hw = struct.unpack("<2H", raw_fn)
        self.assertEqual(hw[0], 0x6880, "ldr r0, [r0, #8] (return palette LUT pointer)")
        self.assertEqual(hw[1], 0x4770, "bx lr")

    def test_08_osd_byte_0x09_unwritten_semantics(self):
        """Byte +0x09 is not written by 0x903111BC; consumer semantics remain UNPROVEN."""
        raw = self.read_bytes(0x903111DE, 6)
        hw = struct.unpack("<3H", raw)
        # 0x903111DE: strb r4, [r1, #8] -> 0x720C
        self.assertEqual(hw[0], 0x720C, "strb r4, [r1, #8]")
        # 0x903111E0: str r0, [r2] -> 0x6010 (writes count, no write to [r1, #9])
        self.assertEqual(hw[1], 0x6010, "str r0, [r2]")
        # 0x903111E2: pop {r4, r5, r6, pc} -> 0xBD70
        self.assertEqual(hw[2], 0xBD70, "pop {r4, r5, r6, pc}")

    def test_09_osd_pipeline_json_consistency(self):
        """verified-osd-pipeline.json must exist with verified stages and unwritten byte 0x09."""
        json_path = Path(__file__).resolve().parents[1] / "reports" / "verified-osd-pipeline.json"
        self.assertTrue(json_path.is_file(), "OSD pipeline JSON must exist")
        data = json.loads(json_path.read_text(encoding="utf-8"))

        self.assertEqual(len(data["stages"]), 5, "Must have exactly 5 pipeline stages")
        self.assertEqual(data["stages"][0]["stride_bytes"], 10)
        self.assertEqual(data["stages"][0]["layout"]["offset_0x09"]["confidence"], "UNPROVEN")
        self.assertEqual(data["stages"][2]["slots"]["+0x04"]["stride_bytes"], 12)


if __name__ == "__main__":
    unittest.main()
