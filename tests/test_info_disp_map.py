import struct
import unittest

from tools.d3300_info_disp_map import (
    ANCHORS,
    ENTRY_COUNT,
    OUTPUT_ADDRESS,
    ROM_BASE,
    TABLE_ADDRESS,
    analyze,
)


class InfoDispMapTest(unittest.TestCase):
    def make_image(self):
        end = max(TABLE_ADDRESS + ENTRY_COUNT * 12, *(a + len(bytes.fromhex(h)) for a, h, _ in ANCHORS))
        image = bytearray(end - ROM_BASE)
        for address, expected_hex, _ in ANCHORS:
            raw = bytes.fromhex(expected_hex)
            image[address - ROM_BASE:address - ROM_BASE + len(raw)] = raw
        for index in range(ENTRY_COUNT):
            struct.pack_into(
                "<III", image, TABLE_ADDRESS - ROM_BASE + index * 12,
                0x2A + index, 1 if index == 20 else 4, OUTPUT_ADDRESS + index * 4,
            )
        return image

    def test_structural_map(self):
        report = analyze(bytes(self.make_image()))
        self.assertTrue(report["anchors_match"])
        self.assertTrue(report["layout_match"])
        self.assertEqual(report["entry_count"], 79)
        self.assertEqual(report["entries"][0]["output_offset"], "0x000")
        self.assertEqual(report["entries"][-1]["output_offset"], "0x138")
        self.assertIsNone(report["entries"][0]["semantic_name"])

    def test_rejects_broken_pointer_stride(self):
        image = self.make_image()
        struct.pack_into("<I", image, TABLE_ADDRESS - ROM_BASE + 5 * 12 + 8, OUTPUT_ADDRESS + 0x100)
        self.assertFalse(analyze(bytes(image))["layout_match"])


if __name__ == "__main__":
    unittest.main()
