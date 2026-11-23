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
        default_candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if default_candidate.is_file():
            fw_path = str(default_candidate)
    if not fw_path or not Path(fw_path).is_file():
        raise unittest.SkipTest(f"Firmware binary b860_102.bin not found at {fw_path}")
    with open(fw_path, "rb") as f:
        data = f.read()
    return data


class TestProvenTables(unittest.TestCase):
    def test_firmware_sha256(self):
        data = get_firmware_data()
        digest = hashlib.sha256(data).hexdigest()
        self.assertEqual(digest, EXPECTED_SHA256)

    def test_property_descriptor_table_0x914ed324(self):
        data = get_firmware_data()
        table_addr = 0x914ED324
        table_off = table_addr - ROM_BASE
        count = 79

        # Verify method 0 of 20CKRRCInfoDispDataGrp at 0x903513AC
        m0_off = 0x903513AC - ROM_BASE
        # 903513ac: movs r0, #0x4f (204f)
        self.assertEqual(data[m0_off:m0_off + 2], bytes.fromhex("4f20"))
        # Literal pointer at 0x903513B8 holds 0x914ED324
        self.assertEqual(
            struct.unpack("<I", data[0x903513B8 - ROM_BASE:0x903513BC - ROM_BASE])[0],
            0x914ED324,
        )

        # Verify entries 0, 1, 2, 3, 4
        e0_prop, e0_size, e0_ram = struct.unpack("<III", data[table_off:table_off + 12])
        self.assertEqual(e0_prop, 0x002A)
        self.assertEqual(e0_size, 4)
        self.assertEqual(e0_ram, 0x104B9044)

        e1_prop, e1_size, e1_ram = struct.unpack("<III", data[table_off + 12:table_off + 24])
        self.assertEqual(e1_prop, 0x002B)
        self.assertEqual(e1_size, 4)
        self.assertEqual(e1_ram, 0x104B9048)

        e2_prop, e2_size, e2_ram = struct.unpack("<III", data[table_off + 24:table_off + 36])
        self.assertEqual(e2_prop, 0x002C)
        self.assertEqual(e2_size, 4)
        self.assertEqual(e2_ram, 0x104B904C)

        e3_prop, e3_size, e3_ram = struct.unpack("<III", data[table_off + 36:table_off + 48])
        self.assertEqual(e3_prop, 0x005A)
        self.assertEqual(e3_size, 4)
        self.assertEqual(e3_ram, 0x104B9050)

        e4_prop, e4_size, e4_ram = struct.unpack("<III", data[table_off + 48:table_off + 60])
        self.assertEqual(e4_prop, 0x002D)
        self.assertEqual(e4_size, 4)
        self.assertEqual(e4_ram, 0x104B9054)

        # Verify all 79 entries are within expected RAM bounds
        for i in range(count):
            entry_off = table_off + i * 12
            p_id, p_sz, p_ram = struct.unpack("<III", data[entry_off:entry_off + 12])
            self.assertIn(p_sz, (1, 2, 4, 8))
            self.assertTrue(0x104B9044 <= p_ram <= 0x104B9180)

    def test_t_kernel_syscall_stubs(self):
        data = get_firmware_data()
        first_stub = 0x90225004
        last_stub = 0x90226060

        stubs = []
        for a in range(0x90225000, 0x90226070, 4):
            w = struct.unpack("<I", data[a - ROM_BASE:a - ROM_BASE + 4])[0]
            if w == 0xE51FF004:  # ldr pc, [pc, #-4] in ARM A32
                stubs.append(a)

        self.assertEqual(len(stubs), 471)
        self.assertEqual(stubs[0], first_stub)
        self.assertEqual(stubs[-1], last_stub)

        # Each stub loads its target pointer from stub_addr + 4
        # Targets are either dynamically loaded RAM handlers or static ROM fallbacks
        for s in stubs:
            target_ptr = struct.unpack("<I", data[s + 4 - ROM_BASE:s + 8 - ROM_BASE])[0]
            is_ram = 0x10000000 <= target_ptr <= 0x10100000
            is_rom = 0x90000000 <= target_ptr <= 0x92000000
            self.assertTrue(is_ram or is_rom)

    def test_osd_command_stride_0x903111bc(self):
        data = get_firmware_data()
        func_off = 0x903111BC - ROM_BASE

        # 903111c4: add.w r5, r0, r0, lsl #2 (eb00 0580) -> r5 = count * 5
        self.assertEqual(data[func_off + 8:func_off + 12], bytes.fromhex("00eb8005"))
        # 903111ca: strh.w r6, [r1, r5, lsl #1] (f821 6015) -> store at [r1 + count * 10]
        self.assertEqual(data[func_off + 14:func_off + 18], bytes.fromhex("21f81560"))
        # 903111de: strb r4, [r1, #8] (720c) -> store byte at +8
        self.assertEqual(data[func_off + 34:func_off + 36], bytes.fromhex("0c72"))

    def test_button_event_handler_0x9013ddcc(self):
        data = get_firmware_data()
        func_off = 0x9013DDCC - ROM_BASE

        # 9013ddde: subs r1, r1, #4 (1f09)
        self.assertEqual(data[func_off + 18:func_off + 20], bytes.fromhex("091f"))
        # 9013dde0: cmp r1, #0x12 (2912)
        self.assertEqual(data[func_off + 20:func_off + 22], bytes.fromhex("1229"))
        # 9013dde4: tbb [pc, r1] (e8df f001)
        self.assertEqual(data[func_off + 24:func_off + 28], bytes.fromhex("dfe801f0"))

    def test_storage_controller_update_0x9013badc(self):
        data = get_firmware_data()
        func_off = 0x9013BADC - ROM_BASE

        # 9013badc: ldr r0, =0x10491580 (pool at 0x9013BED0)
        # 9013bade: str.w r1, [r0, #0xac] (f8c0 10ac)
        self.assertEqual(data[func_off + 2:func_off + 6], bytes.fromhex("c0f8ac10"))
        pool_val = struct.unpack("<I", data[0x9013BED0 - ROM_BASE:0x9013BED4 - ROM_BASE])[0]
        self.assertEqual(pool_val, 0x10491580)

    def test_callback_dispatchers_seams(self):
        data = get_firmware_data()

        # Dispatcher 1 (one-shot): 0x9002702A clears slot before 0x9002702C blx r0
        d1_off = 0x9002702A - ROM_BASE
        # str r7, [r1, #4] (604f)
        self.assertEqual(data[d1_off:d1_off + 2], bytes.fromhex("4f60"))
        # blx r0 (4780)
        self.assertEqual(data[d1_off + 2:d1_off + 4], bytes.fromhex("8047"))

        # Dispatcher 2 (persistent): 0x900271DA blx r0 without clearing store
        d2_off = 0x900271DA - ROM_BASE
        # blx r0 (4780)
        self.assertEqual(data[d2_off:d2_off + 2], bytes.fromhex("8047"))


if __name__ == "__main__":
    unittest.main()
