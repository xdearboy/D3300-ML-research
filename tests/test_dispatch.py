import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_lab import BASE
from d3300_dispatch import trace_tbb


class DispatchTraceTest(unittest.TestCase):
    def test_tbb_targets_and_parallel_arrays(self):
        data = bytearray(0x100)
        instruction = BASE + 0x20
        data[0x20:0x24] = struct.pack("<HH", 0xE8DF, 0xF001)
        data[0x24 + 11:0x24 + 13] = bytes((0x35, 0x37))
        array_a, array_b = BASE + 0x40, BASE + 0x60
        struct.pack_into("<I", data, 0x40 + 11 * 4, 0x1049151C)
        struct.pack_into("<I", data, 0x60 + 12 * 4, 0x10491580)
        result = trace_tbb(data, instruction, 20, [11, 12], [array_a, array_b])
        self.assertEqual([item["target"] for item in result], [BASE + 0x8E, BASE + 0x92])
        self.assertEqual(result[0]["array_values"][f"0x{array_a:08X}"], 0x1049151C)
        self.assertEqual(result[1]["array_values"][f"0x{array_b:08X}"], 0x10491580)

    def test_rejects_bad_instruction_and_index(self):
        data = bytearray(0x20)
        with self.assertRaisesRegex(ValueError, "TBB"):
            trace_tbb(data, BASE, 1, [0])
        data[:4] = struct.pack("<HH", 0xE8DF, 0xF001)
        with self.assertRaisesRegex(ValueError, "index"):
            trace_tbb(data, BASE, 1, [1])


if __name__ == "__main__":
    unittest.main()
