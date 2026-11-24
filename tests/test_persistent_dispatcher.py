import hashlib
import os
import struct
import unittest
from pathlib import Path

EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"
ROM_BASE = 0x90020000


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


class TestPersistentDispatcher(unittest.TestCase):
    def test_firmware_sha256(self):
        data = get_firmware_data()
        digest = hashlib.sha256(data).hexdigest()
        self.assertEqual(digest, EXPECTED_SHA256)

    def test_worker_dispatcher_loop_0x900271a4(self):
        data = get_firmware_data()
        off = 0x900271A4 - ROM_BASE

        # 0x900271A4: ldr.w r8, [pc, #0x184] -> literal 0x9002732C = 0x10121EA8
        self.assertEqual(data[off:off + 4], bytes.fromhex("dff88481"))
        lit_val = struct.unpack("<I", data[0x9002732C - ROM_BASE:0x90027330 - ROM_BASE])[0]
        self.assertEqual(lit_val, 0x10121EA8)

        # 0x900271CE: tst r0, r1 (0842)
        self.assertEqual(data[off + 0x2A:off + 0x2C], bytes.fromhex("0842"))

        # 0x900271DA: blx r0 (8047)
        self.assertEqual(data[off + 0x36:off + 0x38], bytes.fromhex("8047"))

        # 0x900271DC: adds r4, r4, #1 (1c64) - slot pointer NOT cleared
        self.assertEqual(data[off + 0x38:off + 0x3A], bytes.fromhex("641c"))

    def test_one_shot_worker_clears_before_callback(self):
        data = get_firmware_data()
        off = 0x90026FF0 - ROM_BASE

        # 0x9002702A clears slots[r4].func_ptr, then 0x9002702C calls it.
        self.assertEqual(data[off + 0x3A:off + 0x3E],
                         bytes.fromhex("4f608047"))

    def test_one_shot_registration_is_semaphore_protected(self):
        data = get_firmware_data()
        off = 0x900270F8 - ROM_BASE

        # Acquire before scanning, write callback at +4, signal on exit.
        self.assertEqual(data[off + 0x18:off + 0x1C],
                         bytes.fromhex("fef110ef"))
        self.assertEqual(data[off + 0x44:off + 0x48],
                         bytes.fromhex("fef1feee"))
        self.assertEqual(data[off + 0x64:off + 0x68],
                         bytes.fromhex("c1f80490"))

    def test_registration_function_0x900272a6(self):
        data = get_firmware_data()
        off = 0x900272A6 - ROM_BASE

        # 0x900272A6: push.w {r4-r9, sl, lr} (e92d 47f0)
        self.assertEqual(data[off:off + 4], bytes.fromhex("2de9f047"))

        # 0x9002730A: str.w r9, [r1, #4] (f8c1 9004) - write callback to slot
        self.assertEqual(data[off + 0x64:off + 0x68], bytes.fromhex("c1f80490"))

        # 0x90027310: ldr.w r0, [r6, r4, lsl #3] (f856 0034) - load mask as token
        self.assertEqual(data[off + 0x6A:off + 0x6E], bytes.fromhex("56f83400"))

        # Registration takes the dispatcher semaphore before scanning and
        # signals it on both the success and full-table paths.
        self.assertEqual(data[off + 0x18:off + 0x1C], bytes.fromhex("fef13aee"))
        self.assertEqual(data[off + 0x44:off + 0x48], bytes.fromhex("fef128ee"))

    def test_signal_function_0x90027316(self):
        data = get_firmware_data()
        off = 0x90027316 - ROM_BASE

        # 0x90027316: mov r1, r0 (4601)
        self.assertEqual(data[off:off + 2], bytes.fromhex("0146"))

        # 0x90027318: ldr r0, [pc, #0x10] (4804) -> 0x10121EA8
        self.assertEqual(data[off + 2:off + 4], bytes.fromhex("0448"))

        # 0x9002731C: ldr r0, [r0, #4] (6840) -> loads flgid
        self.assertEqual(data[off + 6:off + 8], bytes.fromhex("4068"))

    def test_rom_initial_slots_table(self):
        data = get_firmware_data()
        off_table = 0x0168BE28  # ROM initial image for 0x10121EA8 in kernel_data_2

        # 12-byte header zeroed
        hdr = struct.unpack("<III", data[off_table:off_table + 12])
        self.assertEqual(hdr, (0, 0, 0))

        # 32 slots: each slot i has mask == (1 << i) and func_ptr == 0
        for i in range(32):
            mask, func = struct.unpack(
                "<II",
                data[off_table + 12 + i * 8:off_table + 12 + (i + 1) * 8],
            )
            self.assertEqual(mask, 1 << i)
            self.assertEqual(func, 0)

    def test_registration_callsites_count(self):
        data = get_firmware_data()
        target = 0x900272A6

        callers = []
        for pos in range(0, len(data) - 4, 2):
            hw1 = struct.unpack_from("<H", data, pos)[0]
            if (hw1 & 0xF800) == 0xF000:
                hw2 = struct.unpack_from("<H", data, pos + 2)[0]
                if (hw2 & 0xD000) == 0xD000:
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
                    call_addr = ROM_BASE + pos
                    if (call_addr + 4 + imm32) & 0xFFFFFFFF == target:
                        callers.append(call_addr)

        self.assertEqual(len(callers), 13)
        self.assertIn(0x900275D8, callers)
        self.assertIn(0x9002B920, callers)
        self.assertIn(0x9003A006, callers)
        self.assertIn(0x9003A010, callers)
        self.assertIn(0x900E6A74, callers)
        self.assertIn(0x9011CA7C, callers)
        self.assertIn(0x9011DD78, callers)
        self.assertIn(0x9011F20E, callers)
        self.assertIn(0x90122534, callers)
        self.assertIn(0x90122552, callers)
        self.assertIn(0x90160DF6, callers)
        self.assertIn(0x90161976, callers)
        self.assertIn(0x90162596, callers)


if __name__ == "__main__":
    unittest.main()
