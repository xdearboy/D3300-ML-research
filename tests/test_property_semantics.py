"""Tests for verified CKRRCInfoDispData and PTP property semantics.

Firmware: Nikon D3300 C 1.02 B-firmware (SHA-256 c7eb89f1...)
Audit Scope: Table 0x914ED324, bulk producer 0x9019DE46, and consumer proof chains.
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
TABLE_ADDRESS = 0x914ED324
ENTRY_COUNT = 79
OUTPUT_RAM_BASE = 0x104B9044
OUTPUT_RAM_SIZE = 316

# Proven properties
PROP_BATTERY_STATUS = 0x0040
PROP_ISO_SENSITIVITY = 0x0054
PROP_SHUTTER_SPECULATION = 0x002D

BATTERY_TABLE_ADDR = 0x914FF1F6
ISO_TABLE_ADDR = 0x914FF258


def get_firmware_data() -> bytes:
    """Retrieve verified firmware binary, skipping test if absent."""
    path = os.environ.get("D3300_FIRMWARE")
    if not path:
        default_candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if default_candidate.is_file():
            path = str(default_candidate)
    if not path or not Path(path).is_file():
        raise unittest.SkipTest(f"Firmware binary not found at {path}")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise AssertionError(f"Unexpected firmware SHA-256 hash: {hashlib.sha256(data).hexdigest()}")
    return data


class PropertySemanticsTest(unittest.TestCase):
    """Test suite validating exact byte anchors and semantic proof chains."""

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

    def test_01_firmware_sha256(self):
        """Firmware image must match verified b860_102 SHA-256."""
        data = get_firmware_data()
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

    def test_02_property_table_layout(self):
        """Table 0x914ED324 must have 79 entries covering 0x104B9044..0x104B9180."""
        data = get_firmware_data()
        for i in range(ENTRY_COUNT):
            addr = TABLE_ADDRESS + i * 12
            pid, size, ram_ptr = struct.unpack("<III", data[addr - BASE : addr + 12 - BASE])
            self.assertGreaterEqual(ram_ptr, OUTPUT_RAM_BASE)
            self.assertLess(ram_ptr, OUTPUT_RAM_BASE + OUTPUT_RAM_SIZE)
            self.assertIn(size, (1, 2, 4))

    def test_03_producer_bulk_memcpy(self):
        """Producer at 0x9019DE46 must execute memcpy to 0x104B9044 for 316 bytes."""
        raw = self.read_bytes(0x9019DE44, 16)
        hw = struct.unpack("<8H", raw)
        # 0x9019DE44: add r1, sp, #4 -> 0xA901
        self.assertEqual(hw[0], 0xA901)
        # 0x9019DE46: ldr r0, [0x9019DF80] -> 0x484E
        self.assertEqual(hw[1], 0x484E)
        dest_ptr = self.read_words(0x9019DF80, 1)[0]
        self.assertEqual(dest_ptr, OUTPUT_RAM_BASE)
        # 0x9019DE48: mov.w r2, #0x13C (316 bytes) -> 0xF44F 0x729E
        self.assertEqual((hw[2], hw[3]), (0xF44F, 0x729E))

    def test_04_consumer_dispatch_loop(self):
        """Group getter 0x903513AC must return 79 entries and table pointer 0x914ED324."""
        raw = self.read_bytes(0x903513AC, 14)
        hw = struct.unpack("<7H", raw)
        # 0x903513AC: movs r0, #0x4F (79)
        self.assertEqual(hw[0], 0x204F)
        # 0x903513AE: str r0, [r1]
        self.assertEqual(hw[1], 0x6008)
        # 0x903513B0: ldr r0, [0x903513B8]
        self.assertEqual(hw[2], 0x4801)
        tbl_ptr = self.read_words(0x903513B8, 1)[0]
        self.assertEqual(tbl_ptr, TABLE_ADDRESS)

    def test_05_proven_battery_property_0x0040(self):
        """Property 0x0040 (Index 24) must be proven battery power source status."""
        data = get_firmware_data()
        entry_addr = TABLE_ADDRESS + 24 * 12
        pid, size, ram_ptr = struct.unpack("<III", data[entry_addr - BASE : entry_addr + 12 - BASE])
        self.assertEqual(pid, PROP_BATTERY_STATUS)
        self.assertEqual(size, 4)
        self.assertEqual(ram_ptr, 0x104B90A4)

        # CPTPCBatteryLevel::GetValue at 0x90303310
        # Check call to CINFOManager::GetProperty(0x0040, &sp[4], 4) at 0x90303340..0x9030334A
        raw = self.read_bytes(0x90303340, 12)
        hw = struct.unpack("<6H", raw)
        self.assertEqual(hw[0], 0x2204)  # movs r2, #4
        self.assertEqual(hw[1], 0x2140)  # movs r1, #0x40
        self.assertEqual(hw[2], 0xAB01)  # add r3, sp, #4
        self.assertEqual(hw[3], 0x6807)  # ldr r7, [r0]
        self.assertEqual(hw[4], 0x4628)  # mov r0, r5
        self.assertEqual(hw[5], 0x47B8)  # blx r7

        # Check compare with 2 (external power) and branch to 100%
        branch_bytes = self.read_halfwords(0x90303358, 2)
        self.assertEqual(branch_bytes[0], 0x2802)  # cmp r0, #2
        self.assertEqual(branch_bytes[1], 0xD00F)  # beq 0x9030337C

        # Check battery level lookup table 0x914FF1F6
        table_raw = self.read_bytes(BATTERY_TABLE_ADDR, 8)
        self.assertEqual(list(table_raw), [100, 35, 20, 5, 1, 1, 0, 0])

    def test_06_proven_iso_property_0x0054(self):
        """Property 0x0054 (Index 65) must be proven ISO control sensitivity."""
        data = get_firmware_data()
        entry_addr = TABLE_ADDRESS + 65 * 12
        pid, size, ram_ptr = struct.unpack("<III", data[entry_addr - BASE : entry_addr + 12 - BASE])
        self.assertEqual(pid, PROP_ISO_SENSITIVITY)
        self.assertEqual(size, 4)
        self.assertEqual(ram_ptr, 0x104B9148)

        # CPTPCISOControlSensitivity::GetDevicePropValue at 0x90307218
        # Check call to CINFOManager::GetProperty(0x0054, sp, 4) at 0x9030722E..0x90307236
        raw = self.read_bytes(0x9030722E, 10)
        hw = struct.unpack("<5H", raw)
        self.assertEqual(hw[0], 0x2204)  # movs r2, #4
        self.assertEqual(hw[1], 0x466B)  # mov r3, sp
        self.assertEqual(hw[2], 0x680F)  # ldr r7, [r1]
        self.assertEqual(hw[3], 0x2154)  # movs r1, #0x54
        self.assertEqual(hw[4], 0x47B8)  # blx r7

        # Check clamping to index range 7..67
        clamp_raw = self.read_halfwords(0x9030724C, 8)
        self.assertEqual(clamp_raw[0], 0x2807)  # cmp r0, #7
        self.assertEqual(clamp_raw[1], 0xD201)  # bhs 0x90307254
        self.assertEqual(clamp_raw[2], 0x2007)  # movs r0, #7
        self.assertEqual(clamp_raw[3], 0xE002)  # b 0x9030725A
        self.assertEqual(clamp_raw[4], 0x2843)  # cmp r0, #0x43 (67)
        self.assertEqual(clamp_raw[5], 0xD901)  # bls 0x9030725C
        self.assertEqual(clamp_raw[6], 0x2043)  # movs r0, #0x43
        self.assertEqual(clamp_raw[7], 0x9000)  # str r0, [sp]

        # Check APEX ISO speed table 0x914FF258
        iso_table = self.read_words(ISO_TABLE_ADDR, 68)
        self.assertEqual(iso_table[7], 100)       # Base ISO 100
        self.assertEqual(iso_table[19], 400)      # ISO 400
        self.assertEqual(iso_table[31], 1600)     # ISO 1600
        self.assertEqual(iso_table[43], 6400)     # ISO 6400
        self.assertEqual(iso_table[55], 25600)    # ISO 25600
        self.assertEqual(iso_table[67], 102400)   # Hi-2 max

    def test_07_unproven_shutter_speculation_0x002d(self):
        """Property 0x002D (Index 4) must NOT be queried by PTP exposure handlers."""
        data = get_firmware_data()
        entry_addr = TABLE_ADDRESS + 4 * 12
        pid, size, ram_ptr = struct.unpack("<III", data[entry_addr - BASE : entry_addr + 12 - BASE])
        self.assertEqual(pid, PROP_SHUTTER_SPECULATION)
        self.assertEqual(size, 4)
        self.assertEqual(ram_ptr, 0x104B9054)

        # CPTPCExposureTime::GetDevicePropValue at 0x90300FF8 delegates to helper 0x902FD8F8
        # Helper queries 0x066C, 0x066D, 0x02C1 - never 0x002D
        helper_call = self.read_halfwords(0x902FD926, 4)
        self.assertEqual(helper_call[0], 0xAB02)        # add r3, sp, #8
        self.assertEqual(helper_call[1], 0xF240)        # movw r1, #0x66c
        self.assertEqual(helper_call[2], 0x616C)
        self.assertNotEqual(helper_call[1], 0x212D)

    def test_08_json_ledger_consistency(self):
        """JSON ledger must have exactly 79 entries, 2 PROVEN, and 77 UNPROVEN."""
        json_path = Path("reports/verified-property-semantics.json")
        self.assertTrue(json_path.is_file(), "JSON ledger missing")
        with open(json_path, "r", encoding="utf-8") as f:
            ledger = json.load(f)

        self.assertEqual(ledger["total_entries"], 79)
        self.assertEqual(ledger["proven_count"], 2)
        self.assertEqual(ledger["unproven_count"], 77)
        self.assertEqual(len(ledger["entries"]), 79)

        proven_entries = [e for e in ledger["entries"] if e["confidence"] == "PROVEN"]
        self.assertEqual(len(proven_entries), 2)
        proven_pids = {e["property_id_hex"] for e in proven_entries}
        self.assertEqual(proven_pids, {"0x0040", "0x0054"})

        for entry in ledger["entries"]:
            if entry["property_id_hex"] not in {"0x0040", "0x0054"}:
                self.assertIsNone(entry["semantic_name"])
                self.assertEqual(entry["confidence"], "UNPROVEN")


if __name__ == "__main__":
    unittest.main()
