"""Synthetic tests only: no proprietary Nikon firmware in this repo."""
import importlib.util
import io
import struct
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

LAB_FILE = Path(__file__).resolve().parents[1] / 'tools' / 'd3300_lab.py'
spec = importlib.util.spec_from_file_location('d3300_lab', LAB_FILE)
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)


def test_image():
    # Large enough to include known ROM-to-RAM segments; synthetic only.
    size = max(src - lab.BASE + length for name, src, dst, length, flags in lab.RAM_MAP) + 32
    data = bytearray(size)
    data[:4] = lab.RESET_INSTRUCTION
    struct.pack_into('<I', data, 0x20, lab.BASE + 0x60)
    for i, (_, src, _, length, _) in enumerate(lab.RAM_MAP):
        off = src - lab.BASE
        data[off:off + length] = bytes([0x41 + i]) * length
    data[0x100:0x110] = b'Picture Control!'
    return bytes(data)


class LabTest(unittest.TestCase):
    def test_inspect_synthetic(self):
        obj = lab.analyze(test_image())
        self.assertTrue(obj['all_maps_in_bounds'])
        self.assertEqual(obj['reset_vector'], f'0x{lab.BASE + 0x60:08X}')

    def test_extract_ram(self):
        with tempfile.TemporaryDirectory() as folder:
            img = Path(folder) / 'b860_102.bin'
            img.write_bytes(test_image())
            out = Path(folder) / 'ram'
            lab.main(['extract-ram', str(img), '-o', str(out)])
            for i, (name, src, dst, length, flags) in enumerate(lab.RAM_MAP):
                exported = (out / f'{name}_0x{dst:08X}.bin').read_bytes()
                self.assertEqual(len(exported), length)
                self.assertTrue(all(x == 0x41 + i for x in exported))
            self.assertTrue((out / 'manifest.json').exists())

    def test_bad_file_rejected(self):
        with self.assertRaises(ValueError):
            lab.analyze(b'x' * 0x200)

    def test_no_input_mutation(self):
        with tempfile.TemporaryDirectory() as folder:
            img = Path(folder) / 'b860_102.bin'
            image = test_image()
            img.write_bytes(image)
            with redirect_stdout(io.StringIO()):
                lab.main(['inspect', str(img)])
                lab.main(['find-bytes', str(img), '18 f0 9f e5'])
                lab.main(['strings', str(img), '--contains', 'Picture', '--limit', '5'])
                lab.main(['diff', str(img), str(img)])
            self.assertEqual(img.read_bytes(), image)

if __name__ == '__main__':
    unittest.main()
