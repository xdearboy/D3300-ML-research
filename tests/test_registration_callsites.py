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

TRIGGER_DETAILS = [
    {
        "site": 0x9002749A,
        "type": "BL",
        "load_hw": 0x6860,  # ldr r0, [r4, #4]
        "load_off": -2,
        "slot": 1,
        "mask": 0x00000002,
        "subsystem": "SdDriver.cpp (SD Card Detect / Ch 0)",
        "is_input": False,
    },
    {
        "site": 0x900274E2,
        "type": "BL",
        "load_hw": 0x68A0,  # ldr r0, [r4, #8]
        "load_off": -2,
        "slot": None,
        "mask": 0x00000000,
        "subsystem": "SdDriver.cpp (SD Ch 1 - Unpopulated on D3300)",
        "is_input": False,
    },
    {
        "site": 0x90027554,
        "type": "BL",
        "load_hw": 0x6860,  # ldr r0, [r4, #4]
        "load_off": -2,
        "slot": 1,
        "mask": 0x00000002,
        "subsystem": "SdDriver.cpp (SD Transfer Complete / Ch 0)",
        "is_input": False,
    },
    {
        "site": 0x9002756C,
        "type": "BL",
        "load_hw": 0x6860,  # ldr r0, [r4, #4]
        "load_off": -2,
        "slot": 1,
        "mask": 0x00000002,
        "subsystem": "SdDriver.cpp (SD State Change / Ch 0)",
        "is_input": False,
    },
    {
        "site": 0x9003A3FA,
        "type": "BL",
        "load_hw": 0x6848,  # ldr r0, [r1, #4]
        "load_off": -2,
        "slot": 5,
        "mask": 0x00000020,
        "subsystem": "DspDriver.cpp (DSP Coprocessor Channel A)",
        "is_input": False,
    },
    {
        "site": 0x9003A416,
        "type": "BL",
        "load_hw": 0x6888,  # ldr r0, [r1, #8]
        "load_off": -2,
        "slot": 6,
        "mask": 0x00000040,
        "subsystem": "DspDriver.cpp (DSP Coprocessor Channel B)",
        "is_input": False,
    },
    {
        "site": 0x900E6C5C,
        "type": "BL",
        "load_hw": 0x6C08,  # ldr r0, [r1, #0x40]
        "load_off": -2,
        "slot": 4,
        "mask": 0x00000010,
        "subsystem": "Input Msg Buffer Controller (HDMI-CEC Driver)",
        "is_input": True,  # remote control command packet forwarder
    },
    {
        "site": 0x9011E742,
        "type": "BL",
        "load_hw": 0x6880,  # ldr r0, [r0, #8] (at off-4), cbz r0 (at off-2)
        "load_off": -4,
        "slot": 3,
        "mask": 0x00000008,
        "subsystem": "Input Event Queue Forwarder (GPIO Pin 8 Debouncer)",
        "is_input": True,  # primary hardware input event forwarder
    },
    {
        "site": 0x9011F198,
        "type": "BL",
        "load_hw": 0xF8DA,  # ldr.w r0, [r10, #8] (first halfword of 32-bit Thumb)
        "load_off": -4,
        "slot": 12,
        "mask": 0x00001000,
        "subsystem": "Timer.cpp (Hardware Interval Timer)",
        "is_input": False,
    },
    {
        "site": 0x90122798,
        "type": "BL",
        "load_hw": 0x6860,  # ldr r0, [r4, #4]
        "load_off": -2,
        "slot": 8,
        "mask": 0x00000100,
        "subsystem": "15CUsbDongleCycle (USB Dongle Rx)",
        "is_input": False,
    },
    {
        "site": 0x9015FC24,
        "type": "BL",
        "load_hw": 0x6B80,  # ldr r0, [r0, #0x38]
        "load_off": -2,
        "slot": 10,
        "mask": 0x00000400,
        "subsystem": "I2SCH0 (Audio I2S Channel 0)",
        "is_input": False,
    },
    {
        "site": 0x90161002,
        "type": "BL",
        "load_hw": 0x6A20,  # ldr r0, [r4, #0x20]
        "load_off": -2,
        "slot": 0,
        "mask": 0x00000001,
        "subsystem": "KrDriver.cpp (KRMAIN / Release Sensor GPIO 14.6)",
        "is_input": False,
    },
    {
        "site": 0x9016104E,
        "type": "BL",
        "load_hw": 0x6A20,  # ldr r0, [r4, #0x20]
        "load_off": -2,
        "slot": 0,
        "mask": 0x00000001,
        "subsystem": "KrDriver.cpp (KRMAIN / Release Sensor GPIO 14.5)",
        "is_input": False,
    },
    {
        "site": 0x9016109A,
        "type": "BL",
        "load_hw": 0x6A20,  # ldr r0, [r4, #0x20]
        "load_off": -2,
        "slot": 0,
        "mask": 0x00000001,
        "subsystem": "KrDriver.cpp (KRMAIN / Release Sensor GPIO 14.4)",
        "is_input": False,
    },
    {
        "site": 0x901610E6,
        "type": "BL",
        "load_hw": 0x6A20,  # ldr r0, [r4, #0x20]
        "load_off": -2,
        "slot": 0,
        "mask": 0x00000001,
        "subsystem": "KrDriver.cpp (KRMAIN / Release Sensor GPIO 14.7)",
        "is_input": False,
    },
    {
        "site": 0x90162C8C,
        "type": "BL",
        "load_hw": 0x68E0,  # ldr r0, [r4, #0xC]
        "load_off": -2,
        "slot": 7,
        "mask": 0x00000080,
        "subsystem": "USBMAIN (USB VBUS / Connection Detect)",
        "is_input": False,
    },
]

TAILCALL_TRIGGERS = [
    {
        "site": 0x9002B84A,
        "type": "B.W",
        "load_hw": 0x6940,  # ldr r0, [r0, #0x14]
        "load_off": -2,
        "slot": 11,
        "mask": 0x00000800,
        "subsystem": "14CAudioAACCodec (Audio AAC Codec Buffer)",
    },
    {
        "site": 0x900E6C3E,
        "type": "B.W",
        "load_hw": 0x6C00,  # ldr r0, [r0, #0x40]
        "load_off": -6,
        "slot": 4,
        "mask": 0x00000010,
        "subsystem": "Input Msg Buffer Controller (Overflow / Reset Trigger)",
    },
    {
        "site": 0x9011CFA8,
        "type": "B.W",
        "load_hw": 0x69C0,  # ldr r0, [r0, #0x1C]
        "load_off": -6,
        "slot": 2,
        "mask": 0x00000004,
        "subsystem": "Serial / Display Queue (0x10317FF8)",
    },
    {
        "site": 0x901225FE,
        "type": "B.W",
        "load_hw": 0x6888,  # ldr r0, [r1, #8]
        "load_off": -2,
        "slot": 9,
        "mask": 0x00000200,
        "subsystem": "15CUsbDongleCycle (USB Dongle Tx)",
    },
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
    def test_input_trigger_split_is_explicit(self):
        """Keep GPIO input distinct from external-control packet input."""
        input_sites = {
            item["site"]: item["mask"]
            for item in TRIGGER_DETAILS
            if item.get("is_input")
        }
        self.assertEqual(
            input_sites,
            {
                0x900E6C5C: 0x00000010,  # external control / CEC path
                0x9011E742: 0x00000008,  # GPIO Pin 8 forwarder
            },
        )

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

        for item in TRIGGER_DETAILS:
            site = item["site"]
            off = site - ROM_BASE

            # 1. Verify BL instruction opcode and target
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

            # 2. Verify preceding instruction loading r0
            load_off = item["load_off"]
            actual_hw = struct.unpack("<H", data[off + load_off:off + load_off + 2])[0]
            self.assertEqual(
                actual_hw,
                item["load_hw"],
                f"Preceding load instruction at 0x{site + load_off:08X} for site 0x{site:08X}",
            )

    def test_all_4_tailcall_triggers(self):
        data = get_firmware_data()
        target_trigger = 0x90027316

        for item in TAILCALL_TRIGGERS:
            site = item["site"]
            off = site - ROM_BASE

            # 1. Verify B.W instruction opcode and target (unconditional branch: 1111 0 S imm10 10 J1 1 J2 imm11)
            hw1, hw2 = struct.unpack("<HH", data[off:off + 4])
            self.assertEqual(hw1 & 0xF800, 0xF000, f"B.W hw1 at 0x{site:08X}")
            self.assertEqual(hw2 & 0xD000, 0x9000, f"B.W hw2 at 0x{site:08X}")

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
                f"Tail-call at 0x{site:08X} must target 0x{target_trigger:08X}",
            )

            # 2. Verify preceding load instruction
            load_off = item["load_off"]
            actual_hw = struct.unpack("<H", data[off + load_off:off + load_off + 2])[0]
            self.assertEqual(
                actual_hw,
                item["load_hw"],
                f"Tail-call load instruction at 0x{site + load_off:08X} for site 0x{site:08X}",
            )

    def test_complete_slot_trigger_coverage(self):
        # Assert all 13 slots (0..12) have a verified trigger path in ROM
        triggered_slots = set()
        for item in TRIGGER_DETAILS:
            if item["slot"] is not None:
                triggered_slots.add(item["slot"])
        for item in TAILCALL_TRIGGERS:
            if item["slot"] is not None:
                triggered_slots.add(item["slot"])

        all_expected_slots = set(range(13))
        self.assertEqual(triggered_slots, all_expected_slots, "All slots 0..12 must have proven triggers")

    def test_input_gpio_pin8_forwarder_chain(self):
        data = get_firmware_data()

        # Trigger 8 at 0x9011E742:
        # 0x9011E738: push {r4, lr}
        # 0x9011E73A: mov r4, r0
        # 0x9011E73C: ldr r0, [pc, #0x178] -> [0x9011E8B8] = 0x1011FFB4
        # 0x9011E73E: ldr r0, [r0, #8] (opcode 0x6880)
        # 0x9011E740: cbz r0, 0x9011E746 (opcode 0xB108)
        # 0x9011E742: bl 0x90027316
        site = 0x9011E742
        off = site - ROM_BASE

        ldr_hw = struct.unpack("<H", data[off - 4:off - 2])[0]
        cbz_hw = struct.unpack("<H", data[off - 2:off])[0]
        self.assertEqual(ldr_hw, 0x6880, "ldr r0, [r0, #8]")
        self.assertEqual(cbz_hw, 0xB108, "cbz r0, 0x9011E746")

        # GPIO debouncer call-sites to 0x9011E738:
        # 0x9011DDFA: bl 0x9011E738 (assert pin)
        # 0x9011DE0E: bl 0x9011E738 (deassert pin)
        for caller_site in (0x9011DDFA, 0x9011DE0E):
            c_off = caller_site - ROM_BASE
            hw1, hw2 = struct.unpack("<HH", data[c_off:c_off + 4])
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
            dest = (caller_site + 4 + imm32) & 0xFFFFFFFF
            self.assertEqual(dest, 0x9011E738, f"Debouncer caller at 0x{caller_site:08X}")

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
