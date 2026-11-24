"""Tests verifying the CKRRCInfoDispData property map table 0x914ED324,

bulk memcpy producer at 0x9019DE46, and consumer loop at 0x90169A28.
"""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import struct
import unittest

BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
TABLE_ADDRESS = 0x914ED324
ENTRY_COUNT = 79
RAM_BASE = 0x104B9044


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


class PropertyConsumersTest(unittest.TestCase):
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

    def test_firmware_sha256(self):
        data = firmware()
        self.assertEqual(hashlib.sha256(data).hexdigest(), EXPECTED_SHA256)

    def test_property_table_79_entries(self):
        data = firmware()
        for i in range(ENTRY_COUNT):
            addr = TABLE_ADDRESS + i * 12
            pid, size, ram_ptr = struct.unpack("<III", data[addr - BASE : addr + 12 - BASE])
            self.assertGreaterEqual(ram_ptr, RAM_BASE)
            self.assertLessEqual(ram_ptr, RAM_BASE + 0x138)
            self.assertIn(size, (1, 2, 4))

        # Check audited properties
        # Index 0: 0x002A at 0x104B9044
        pid0, sz0, ptr0 = struct.unpack("<III", data[TABLE_ADDRESS - BASE : TABLE_ADDRESS + 12 - BASE])
        self.assertEqual(pid0, 0x002A)
        self.assertEqual(sz0, 4)
        self.assertEqual(ptr0, 0x104B9044)

        # Index 1: 0x002B at 0x104B9048
        pid1, sz1, ptr1 = struct.unpack("<III", data[TABLE_ADDRESS + 12 - BASE : TABLE_ADDRESS + 24 - BASE])
        self.assertEqual(pid1, 0x002B)
        self.assertEqual(sz1, 4)
        self.assertEqual(ptr1, 0x104B9048)

        # Index 2: 0x002C at 0x104B904C
        pid2, sz2, ptr2 = struct.unpack("<III", data[TABLE_ADDRESS + 24 - BASE : TABLE_ADDRESS + 36 - BASE])
        self.assertEqual(pid2, 0x002C)
        self.assertEqual(sz2, 4)
        self.assertEqual(ptr2, 0x104B904C)

        # Index 3: 0x005A at 0x104B9050
        pid3, sz3, ptr3 = struct.unpack("<III", data[TABLE_ADDRESS + 36 - BASE : TABLE_ADDRESS + 48 - BASE])
        self.assertEqual(pid3, 0x005A)
        self.assertEqual(sz3, 4)
        self.assertEqual(ptr3, 0x104B9050)

        # Index 4: 0x002D at 0x104B9054
        pid4, sz4, ptr4 = struct.unpack("<III", data[TABLE_ADDRESS + 48 - BASE : TABLE_ADDRESS + 60 - BASE])
        self.assertEqual(pid4, 0x002D)
        self.assertEqual(sz4, 4)
        self.assertEqual(ptr4, 0x104B9054)

        # Index 5: 0x005D at 0x104B9058
        pid5, sz5, ptr5 = struct.unpack("<III", data[TABLE_ADDRESS + 60 - BASE : TABLE_ADDRESS + 72 - BASE])
        self.assertEqual(pid5, 0x005D)
        self.assertEqual(sz5, 4)
        self.assertEqual(ptr5, 0x104B9058)

    def test_producer_bulk_memcpy_0x9019de46(self):
        raw = self.read_bytes(0x9019DE44, 16)
        hw = struct.unpack("<8H", raw)

        # 0x9019DE44: add r1, sp, #4 -> 0xA901
        self.assertEqual(hw[0], 0xA901, "add r1, sp, #4")
        # 0x9019DE46: ldr r0, [0x9019DF80] = 0x104B9044 -> 0x484E
        self.assertEqual(hw[1], 0x484E, "ldr r0, [literal pool]")
        literal_dest = self.read_words(0x9019DF80, 1)[0]
        self.assertEqual(literal_dest, RAM_BASE, "Copy destination is 0x104B9044")

        # 0x9019DE48: mov.w r2, 0x13c -> 0xF44F, 0x729E
        self.assertEqual((hw[2], hw[3]), (0xF44F, 0x729E), "mov.w r2, 0x13c (316 bytes)")

    def test_group_getter_0x903513ac(self):
        raw = self.read_bytes(0x903513AC, 14)
        hw = struct.unpack("<7H", raw)

        # 0x903513AC: movs r0, #0x4F (79) -> 0x204F
        self.assertEqual(hw[0], 0x204F, "movs r0, #0x4f (79 entries)")
        # 0x903513AE: str r0, [r1] -> 0x6008
        self.assertEqual(hw[1], 0x6008, "str r0, [r1]")
        # 0x903513B0: ldr r0, [0x903513B8] = 0x914ED324 -> 0x4801
        self.assertEqual(hw[2], 0x4801, "ldr r0, [0x903513B8]")
        # 0x903513B2: str r0, [r2] -> 0x6010
        self.assertEqual(hw[3], 0x6010, "str r0, [r2]")
        # 0x903513B4: bx lr -> 0x4770
        self.assertEqual(hw[4], 0x4770, "bx lr")

        literal_table = self.read_words(0x903513B8, 1)[0]
        self.assertEqual(literal_table, TABLE_ADDRESS, "Table pointer is 0x914ED324")

    def test_consumer_notification_and_dispatch(self):
        # 0x9019DE50..0x9019DE5E:
        # ldr.w r0, [sl, #0x2c0] (listener CINFOManager)
        # ldr r1, [r0]
        # ldr r2, [r1, #0xc] (slot +0x0C)
        # add.w r1, sl, #0x2bc (&this->info_disp_data_grp)
        # blx r2
        raw = self.read_bytes(0x9019DE50, 16)
        hw = struct.unpack("<8H", raw)
        self.assertEqual((hw[0], hw[1]), (0xF8DA, 0x02C0), "ldr.w r0, [sl, #0x2c0]")
        self.assertEqual(hw[2], 0x6801, "ldr r1, [r0]")
        self.assertEqual(hw[3], 0x68CA, "ldr r2, [r1, #0xc]")
        self.assertEqual((hw[4], hw[5]), (0xF50A, 0x712F), "add.w r1, sl, #0x2bc")
        self.assertEqual(hw[6], 0x4790, "blx r2")

        # CINFOManager slot +0x0C at 0x91516DB4 + 0x0C points to 0x90166252 (Thumb 0x90166253)
        slot0c = self.read_words(0x91516DB4 + 0x0C, 1)[0]
        self.assertEqual(slot0c, 0x90166253)


if __name__ == "__main__":
    unittest.main()
