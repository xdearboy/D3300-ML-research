"""Tests for verified PTP exposure, shutter speed, aperture, and white balance semantics.

Firmware: Nikon D3300 C 1.02 B-firmware (SHA-256 c7eb89f1...)
Audit Scope: Exposure helper 0x902FD8F8, aperture helper 0x90301B50, white balance 0x90302864,
and their respective conversion tables and PTP serialization routines.
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

SHUTTER_HELPER_ADDR = 0x902FD8F8
APERTURE_HELPER_ADDR = 0x90301B50
WHITE_BALANCE_METHOD_ADDR = 0x90302864

EXPOSURE_TABLE_ADDR = 0x914FE9FC
APERTURE_TABLE_ADDR = 0x914FED48
WB_TABLE_ADDR = 0x914FF1EE


def get_firmware_data() -> bytes:
    path = os.environ.get("D3300_FIRMWARE")
    if not path:
        default_candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if default_candidate.is_file():
            path = str(default_candidate)
    if not path or not Path(path).is_file():
        raise unittest.SkipTest(f"Firmware binary not found at {path}")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise AssertionError(f"Unexpected firmware hash: {hashlib.sha256(data).hexdigest()}")
    return data


def thumb_bl_target(address: int, raw: bytes) -> int:
    """Decode a T32 BL and return its absolute target; reject non-BL encodings."""
    hw1, hw2 = struct.unpack("<HH", raw)
    if (hw1 & 0xF800) != 0xF000 or (hw2 & 0xD000) != 0xD000:
        raise AssertionError(f"not a Thumb BL at 0x{address:08X}")
    s = (hw1 >> 10) & 1
    i1 = (~(((hw2 >> 13) & 1) ^ s)) & 1
    i2 = (~(((hw2 >> 11) & 1) ^ s)) & 1
    displacement = ((s << 24) | (i1 << 23) | (i2 << 22) |
                    ((hw1 & 0x3FF) << 12) | ((hw2 & 0x7FF) << 1))
    if s:
        displacement -= 1 << 25
    return address + 4 + displacement


class PTPExposureSemanticsTest(unittest.TestCase):
    def read_bytes(self, addr: int, size: int) -> bytes:
        data = get_firmware_data()
        off = addr - BASE
        return data[off : off + size]

    def read_halfwords(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 2)
        return struct.unpack(f"<{count}H", raw)

    def read_words(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 4)
        return struct.unpack(f"<{count}I", raw)

    def test_01_shutter_classes_call_shared_helper(self):
        """Both CPTPCExposureTime and CPTPCShutterSpeed must call helper 0x902FD8F8."""
        # CPTPCExposureTime::GetDevicePropValue at 0x9030100C calls 0x902FD8F8
        raw_exp = self.read_bytes(0x9030100C, 4)
        self.assertEqual(thumb_bl_target(0x9030100C, raw_exp), SHUTTER_HELPER_ADDR)

        # CPTPCShutterSpeed::GetDevicePropValue at 0x902E4548 calls 0x902FD8F8
        raw_shutter = self.read_bytes(0x902E4548, 4)
        self.assertEqual(thumb_bl_target(0x902E4548, raw_shutter), SHUTTER_HELPER_ADDR)

    def test_02_shutter_helper_queries_core_properties(self):
        """Helper 0x902FD8F8 must query properties 0x066C, 0x066D, 0x02C1."""
        # 0x902FD926: add r3, sp, #8
        # 0x902FD928: movw r1, #0x66c
        hw = self.read_halfwords(0x902FD926, 4)
        self.assertEqual(hw[0], 0xAB02)  # add r3, sp, #8
        self.assertEqual(hw[1], 0xF240)  # movw r1, 0x66c
        self.assertEqual(hw[2], 0x616C)

        # 0x902FD940: mov r3, sp
        # 0x902FD942: movw r1, #0x66d
        hw2 = self.read_halfwords(0x902FD940, 4)
        self.assertEqual(hw2[0], 0x466B)  # mov r3, sp
        self.assertEqual(hw2[1], 0xF240)  # movw r1, 0x66d
        self.assertEqual(hw2[2], 0x616D)

        # 0x902FD954: add r3, sp, #12
        # 0x902FD956: movw r1, #0x2c1
        hw3 = self.read_halfwords(0x902FD954, 4)
        self.assertEqual(hw3[0], 0xAB03)  # add r3, sp, #12
        self.assertEqual(hw3[1], 0xF240)  # movw r1, 0x2c1
        self.assertEqual(hw3[2], 0x21C1)

    def test_03_shutter_subhelpers_query_gating_and_sync(self):
        """Helper 0x902FD542 queries 0x066B; helper 0x902FD4A0 queries 0x00BD, 0x00B9, 0x02D6."""
        # 0x902FD584: movw r1, #0x66b
        hw_gating = self.read_halfwords(0x902FD584, 2)
        self.assertEqual(hw_gating[0], 0xF240)
        self.assertEqual(hw_gating[1], 0x616B)

        # 0x902FD4CA: movs r1, #0xbd
        # 0x902FD4DC: movs r1, #0xb9
        # 0x902FD4F0: movw r1, #0x2d6
        hw_sync1 = self.read_halfwords(0x902FD4CA, 1)[0]
        self.assertEqual(hw_sync1, 0x21BD)

        hw_sync2 = self.read_halfwords(0x902FD4DC, 1)[0]
        self.assertEqual(hw_sync2, 0x21B9)

        hw_sync3 = self.read_halfwords(0x902FD4F0, 2)
        self.assertEqual(hw_sync3[0], 0xF240)
        self.assertEqual(hw_sync3[1], 0x21D6)

    def test_04_exposure_time_conversion_table(self):
        """Table 0x914FE9FC must contain verified microsecond exposure times."""
        times = self.read_words(EXPOSURE_TABLE_ADDR, 8)
        self.assertEqual(times[0], 300000)  # 30.0 sec
        self.assertEqual(times[1], 250000)  # 25.0 sec
        self.assertEqual(times[2], 250000)
        self.assertEqual(times[3], 200000)  # 20.0 sec
        self.assertEqual(times[4], 200000)
        self.assertEqual(times[5], 150000)  # 15.0 sec
        self.assertEqual(times[6], 150000)
        self.assertEqual(times[7], 130000)  # 13.0 sec

    def test_05_aperture_helper_queries_0x066e_and_fnumber_table(self):
        """Helper 0x90301B50 must query property 0x066E and index table 0x914FED48."""
        # 0x90301B7C: movw r1, #0x66e
        hw = self.read_halfwords(0x90301B7C, 2)
        self.assertEqual(hw[0], 0xF240)
        self.assertEqual(hw[1], 0x616E)

        # 0x90301E54 points to 0x914FED48
        table_ptr = self.read_words(0x90301E54, 1)[0]
        self.assertEqual(table_ptr, APERTURE_TABLE_ADDR)

        # F-number table entries (F * 100)
        f_numbers = self.read_halfwords(APERTURE_TABLE_ADDR, 35)
        self.assertEqual(f_numbers[0], 100)   # f/1.0
        self.assertEqual(f_numbers[6], 140)   # f/1.4
        self.assertEqual(f_numbers[12], 200)  # f/2.0
        self.assertEqual(f_numbers[18], 280)  # f/2.8
        self.assertEqual(f_numbers[24], 400)  # f/4.0
        self.assertEqual(f_numbers[30], 560)  # f/5.6

    def test_06_white_balance_queries_0x066a_and_mapping_table(self):
        """CPTPCWhiteBalance::GetDevicePropValue must query 0x066A and map via 0x914FF1EE."""
        # 0x9030289A: movw r1, #0x66a
        hw = self.read_halfwords(0x9030289A, 2)
        self.assertEqual(hw[0], 0xF240)
        self.assertEqual(hw[1], 0x616A)

        # 0x90302B3C points to 0x914FF1EE
        table_ptr = self.read_words(0x90302B3C, 1)[0]
        self.assertEqual(table_ptr, WB_TABLE_ADDR)

        # Mapping table entries
        wb_map = list(self.read_bytes(WB_TABLE_ADDR, 8))
        self.assertEqual(wb_map, [0, 3, 2, 1, 4, 5, 6, 7])

    def test_07_json_structure_and_isolation(self):
        """JSON report must be structurally complete and isolate properties from InfoDisp."""
        report_path = Path("reports/verified-ptp-exposure-semantics.json")
        self.assertTrue(report_path.is_file(), "Report JSON missing")
        with open(report_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        self.assertTrue(data["infodisp_isolation_confirmed"])
        props = {p["property_id_hex"]: p for p in data["properties"]}
        self.assertEqual(props["0x066C"]["role"], "shutter_speed_step_index")
        self.assertEqual(props["0x066E"]["role"], "aperture_fnumber_step_index")
        self.assertEqual(props["0x066A"]["role"], "white_balance_mode_id")
        self.assertIsNone(props["0x002D"]["role"])
        self.assertEqual(props["0x002D"]["confidence"], "UNPROVEN")


if __name__ == "__main__":
    unittest.main()
