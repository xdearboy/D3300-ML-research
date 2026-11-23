import hashlib
import os
import struct
import unittest
from pathlib import Path

EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
ROM_BASE = 0x90020000

# Verified 13 registration call-sites
CALLSITES_PROVEN = [
    {
        "site": 0x90161976,
        "callback": 0x90160FA0,
        "thumb_entry": 0x90160FA1,
        "store_hw": 0x6238,  # str r0, [r7, #0x20]
        "slot": 0,
        "mask": 0x00000001,
    },
    {
        "site": 0x900275D8,
        "callback": 0x9002758A,
        "thumb_entry": 0x9002758B,
        "store_hw": 0x6060,  # str r0, [r4, #4]
        "slot": 1,
        "mask": 0x00000002,
    },
    {
        "site": 0x9011CA7C,
        "callback": 0x9011CA1E,
        "thumb_entry": 0x9011CA1F,
        "store_hw": 0x61E8,  # str r0, [r5, #0x1C]
        "slot": 2,
        "mask": 0x00000004,
    },
    {
        "site": 0x9011DD78,
        "callback": 0x9011DD5C,
        "thumb_entry": 0x9011DD5D,
        "store_hw": 0x60A0,  # str r0, [r4, #8]
        "slot": 3,
        "mask": 0x00000008,
    },
    {
        "site": 0x900E6A74,
        "callback": 0x900E6A08,
        "thumb_entry": 0x900E6A09,
        "store_hw": 0x6408,  # str r0, [r1, #0x40]
        "slot": 4,
        "mask": 0x00000010,
    },
    {
        "site": 0x9003A006,
        "callback": 0x90039FA8,
        "thumb_entry": 0x90039FA9,
        "store_hw": 0x6068,  # str r0, [r5, #4]
        "slot": 5,
        "mask": 0x00000020,
    },
    {
        "site": 0x9003A010,
        "callback": 0x90039F98,
        "thumb_entry": 0x90039F99,
        "store_hw": 0x60A8,  # str r0, [r5, #8]
        "slot": 6,
        "mask": 0x00000040,
    },
    {
        "site": 0x90162596,
        "callback": 0x90162506,
        "thumb_entry": 0x90162507,
        "store_hw": 0x60E0,  # str r0, [r4, #0xC]
        "slot": 7,
        "mask": 0x00000080,
    },
    {
        "site": 0x90122534,
        "callback": 0x901224C6,
        "thumb_entry": 0x901224C7,
        "store_hw": 0x6060,  # str r0, [r4, #4]
        "slot": 8,
        "mask": 0x00000100,
    },
    {
        "site": 0x90122552,
        "callback": 0x90122444,
        "thumb_entry": 0x90122445,
        "store_hw": 0x60A0,  # str r0, [r4, #8]
        "slot": 9,
        "mask": 0x00000200,
    },
    {
        "site": 0x90160DF6,
        "callback": 0x9015FC2E,
        "thumb_entry": 0x9015FC2F,
        "store_hw": 0xF8C8,  # str.w r0, [r8, #0x38]
        "slot": 10,
        "mask": 0x00000400,
    },
    {
        "site": 0x9002B920,
        "callback": 0x9002B942,
        "thumb_entry": 0x9002B943,
        "store_hw": 0x6160,  # str r0, [r4, #0x14]
        "slot": 11,
        "mask": 0x00000800,
    },
    {
        "site": 0x9011F20E,
        "callback": 0x9011F080,
        "thumb_entry": 0x9011F081,
        "store_hw": 0x60B0,  # str r0, [r6, #8]
        "slot": 12,
        "mask": 0x00001000,
    },
]

TRIGGERS_PROVEN = [
    0x9002749A, 0x900274E2, 0x90027554, 0x9002756C,
    0x9003A3FA, 0x9003A416, 0x900E6C5C, 0x9011E742,
    0x9011F198, 0x90122798, 0x9015FC24, 0x90161002,
    0x9016104E, 0x9016109A, 0x901610E6, 0x90162C8C,
]


def get_firmware_data():
    fw_path = os.environ.get("D3300_FIRMWARE")
    if not fw_path:
        candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if candidate.is_file():
            fw_path = str(candidate)
    if not fw_path or not Path(fw_path).is_file():
        raise unittest.SkipTest(f"Firmware binary b860_102.bin not found at {fw_path}")
    with open(fw_path, "rb") as f:
        data = f.read()
    return data


class TestRegistrationCallsites(unittest.TestCase):
    def test_firmware_sha256(self):
        data = get_firmware_data()
        digest = hashlib.sha256(data).hexdigest()
        self.assertEqual(digest, EXPECTED_SHA256)

    def test_all_13_registration_callsites(self):
        data = get_firmware_data()
        target_reg = 0x900272A6

        for item in CALLSITES_PROVEN:
            site = item["site"]
            off = site - ROM_BASE

            # Verify BL instruction target
            hw1, hw2 = struct.unpack("<HH", data[off:off + 4])
            s = (hw1 >> 10) & 1
            j1 = (hw2 >> 13) & 1
            j2 = (hw2 >> 11) & 1
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm10 = hw1 & 0x3FF
            imm11 = hw2 & 0x7FF
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 -= 1 << 25
            resolved_target = (site + 4 + imm32) & 0xFFFFFFFF
            self.assertEqual(resolved_target, target_reg, f"Call at 0x{site:08X} target")

            # Verify callback function address in ROM
            cb_off = item["callback"] - ROM_BASE
            self.assertTrue(0 <= cb_off < len(data) - 4)

            # Verify token store instruction following call
            after_hw = struct.unpack("<H", data[off + 4:off + 6])[0]
            # Some stores are immediately at off+4, others at off+8..off+14
            nearby_hws = [
                struct.unpack("<H", data[off + k:off + k + 2])[0]
                for k in range(4, 20, 2)
            ]
            self.assertIn(
                item["store_hw"],
                nearby_hws,
                f"Token store 0x{item['store_hw']:04X} near 0x{site:08X}",
            )

    def test_all_16_trigger_callsites(self):
        data = get_firmware_data()
        target_trigger = 0x90027316

        for site in TRIGGERS_PROVEN:
            off = site - ROM_BASE
            hw1, hw2 = struct.unpack("<HH", data[off:off + 4])
            s = (hw1 >> 10) & 1
            j1 = (hw2 >> 13) & 1
            j2 = (hw2 >> 11) & 1
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm10 = hw1 & 0x3FF
            imm11 = hw2 & 0x7FF
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 -= 1 << 25
            resolved_target = (site + 4 + imm32) & 0xFFFFFFFF
            self.assertEqual(
                resolved_target,
                target_trigger,
                f"Trigger call at 0x{site:08X} target",
            )

    def test_boot_initialization_order(self):
        data = get_firmware_data()

        # In DriverInit.cpp:
        # 0x900223AC: inits RelayEventTaskEx (0x900271E4)
        # 0x900223BC: inits KRMAIN (0x90161760) -> Slot 0
        # 0x900224C8: inits SdDriver (0x900275AC) -> Slot 1
        # 0x900224F8: inits Input Msg Buffer (0x900E6A28) -> Slot 4
        # 0x90022538: inits DspDriver (0x90039FBA) -> Slots 5, 6
        # 0x90022560: inits USBMAIN (0x9016255A) -> Slots 7, 8, 9
        # 0x90022584: inits Audio (0x9015FC3C) -> Slots 10, 11
        # 0x900225D8: inits Timer (0x9011F1A6) -> Slot 12

        seq_calls = [
            (0x900223AC, 0x900271E4),
            (0x900223BC, 0x90161760),
            (0x900224C8, 0x900275AC),
            (0x900224F8, 0x900E6A28),
            (0x90022538, 0x90039FBA),
            (0x90022560, 0x9016255A),
            (0x90022584, 0x9015FC3C),
            (0x900225D8, 0x9011F1A6),
        ]

        for call_site, expected_target in seq_calls:
            off = call_site - ROM_BASE
            hw1, hw2 = struct.unpack("<HH", data[off:off + 4])
            s = (hw1 >> 10) & 1
            j1 = (hw2 >> 13) & 1
            j2 = (hw2 >> 11) & 1
            i1 = ~(j1 ^ s) & 1
            i2 = ~(j2 ^ s) & 1
            imm10 = hw1 & 0x3FF
            imm11 = hw2 & 0x7FF
            imm32 = (s << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1)
            if s:
                imm32 -= 1 << 25
            resolved = (call_site + 4 + imm32) & 0xFFFFFFFF
            self.assertEqual(
                resolved,
                expected_target,
                f"Boot call at 0x{call_site:08X} expected 0x{expected_target:08X}",
            )


if __name__ == "__main__":
    unittest.main()
