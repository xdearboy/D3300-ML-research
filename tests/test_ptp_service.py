#!/usr/bin/env python3
"""Unit tests for tools/d3300_ptp_service.py."""

import struct
import tempfile
import unittest
from tools.d3300_ptp_service import (
    ROM_BASE,
    analyze_firmware_ptp,
    load_binary,
    scan_ptp_vendor_opcodes,
    scan_service_subopcodes,
)


class TestPtpService(unittest.TestCase):
    def test_synthetic_scan(self) -> None:
        # Build synthetic buffer with 0x94FE, 0x94FF, 0xF001, 0xF036, 0xF037
        buf = bytearray(0x200)
        struct.pack_into("<H", buf, 0x10, 0x94FE)
        struct.pack_into("<I", buf, 0x12, 0x91054321)  # target func
        struct.pack_into("<H", buf, 0x20, 0x94FF)
        struct.pack_into("<H", buf, 0x30, 0xF001)
        struct.pack_into("<H", buf, 0x40, 0xF036)
        struct.pack_into("<H", buf, 0x50, 0xF037)

        vendor_ops = scan_ptp_vendor_opcodes(bytes(buf), ROM_BASE)
        self.assertEqual(len(vendor_ops), 2)
        self.assertEqual(vendor_ops[0]["opcode"], "0x94FE")
        self.assertEqual(vendor_ops[0]["name"], "CANDIDATE_SERVICE_WRAP_SEND")
        self.assertEqual(vendor_ops[0]["evidence"], "raw_halfword_occurrence")
        self.assertEqual(vendor_ops[0]["target_func_after"], "0x91054321")
        self.assertEqual(vendor_ops[1]["opcode"], "0x94FF")

        subops = scan_service_subopcodes(bytes(buf), ROM_BASE)
        self.assertEqual(len(subops), 3)
        self.assertEqual(subops[0]["subop"], "0xF001")
        self.assertEqual(subops[0]["evidence"], "raw_halfword_occurrence")
        self.assertEqual(subops[1]["subop"], "0xF036")
        self.assertEqual(subops[2]["subop"], "0xF037")

        report = analyze_firmware_ptp(bytes(buf), ROM_BASE)
        self.assertEqual(report["vendor_opcodes_found"], 2)
        self.assertEqual(report["service_subops_found"], 3)
        self.assertIn("no D3300 PTP command semantics", report["warning"])

    def test_load_binary_plain(self) -> None:
        data = b"\x00" * 64
        with tempfile.NamedTemporaryFile(delete=True) as tmp:
            tmp.write(data)
            tmp.flush()
            loaded, base = load_binary(tmp.name)
            self.assertEqual(loaded, data)
            self.assertEqual(base, ROM_BASE)


if __name__ == "__main__":
    unittest.main()
