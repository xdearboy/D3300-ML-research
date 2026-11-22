"""Decoder regression tests: synthetic buffers only, no Nikon binaries."""
import sys
import struct
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import decode_nikon_package as fw


class DecodePackageTest(unittest.TestCase):
    def test_xor_tables_have_correct_size(self):
        self.assertEqual(tuple(map(len, (fw.ORD1, fw.ORD2, fw.ORD3))), (256, 256, 256))
        self.assertEqual(fw.ORD3[10], 0)  # Nikon Hacker's integer 0x100 -> 0 when written as byte

    def test_roundtrip_crossing_layer_boundaries(self):
        blob = bytes((i * 157 + i // 17) & 255 for i in range(140000))
        enc = fw.unxor(blob)
        self.assertNotEqual(blob, enc)
        self.assertEqual(fw.unxor(enc), blob)
        self.assertEqual(len(enc), len(blob))

    def test_decode_and_parse_fake_two_component_package(self):
        decoded = bytearray(0x3000)
        # 32-byte package signature area, then big-endian Nikon header.
        struct.pack_into('>IIII', decoded, 0x20, 2, 0x70, 0, 0)
        for j, (name, off, size) in enumerate((('fake_a.bin', 0x70, 0x800), ('fake_b.bin', 0x870, 0x900))):
            decoded[0x30+j*32:0x30+j*32+16] = name.encode().ljust(16, b'\0')
            struct.pack_into('>IIII', decoded, 0x30+j*32+16, off, size, 0, 0)
            decoded[off:off+size] = b'\x55' * size
        encrypted = fw.unxor(decoded)
        result = fw.unxor(encrypted)
        info = fw.find_components(result)
        self.assertEqual(info['count'], 2)
        self.assertEqual(info['endian'], 'big')
        self.assertEqual([x['name'] for x in info['entries']], ['fake_a.bin', 'fake_b.bin'])
        self.assertEqual([x['length'] for x in info['entries']], [0x800, 0x900])

    def test_refuses_invalid_header(self):
        with self.assertRaises(ValueError):
            fw.find_components(bytes(200))
        with self.assertRaises(ValueError):
            fw.unxor(b'\x00', -1)

if __name__ == '__main__':
    unittest.main()
