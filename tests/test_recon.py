"""Read-only tests using generated SYNTHETIC images, never Nikon binaries."""
import importlib.util
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import d3300_recon as recon
import d3300_elf as elf
from d3300_lab import BASE, RAM_MAP, RESET_INSTRUCTION


def fake_image(*, modified=(), data_size=None):
    minimum = max(x['offset'] + len(x['original']) for x in recon.sites()) + 0x100
    if data_size is None:
        data_size = minimum
    b = bytearray(data_size)
    b[:4] = RESET_INSTRUCTION
    struct.pack_into('<I', b, 0x20, BASE + 0x80)
    for s in recon.sites():
        v = s['patched'] if s['group'] in modified else s['original']
        start = s['offset']
        b[start:start + len(v)] = v
    return bytes(b)


class ReconTest(unittest.TestCase):
    def test_23_anchor_locations_and_consistency(self):
        ss = recon.sites()
        self.assertEqual(len(ss), 23)
        self.assertEqual(len(set(x['offset'] for x in ss)), 23)
        self.assertEqual(len([x for x in ss if x['group'] == 'video_64mbps']), 20)
        self.assertEqual(len([x for x in ss if x['group'] == 'liveview_timeout']), 3)

    def test_original_both_groups(self):
        result = recon.audit(fake_image())
        self.assertEqual(result['groups']['video_64mbps']['status'], 'original')
        self.assertEqual(result['groups']['liveview_timeout']['status'], 'original')

    def test_patched_group_does_not_confuse_other_group(self):
        result = recon.audit(fake_image(modified=('liveview_timeout',)))
        self.assertEqual(result['groups']['video_64mbps']['status'], 'original')
        self.assertEqual(result['groups']['liveview_timeout']['status'], 'nikon_hacker_patched')

    def test_single_site_unknown_instead_of_claiming_original(self):
        blob = bytearray(fake_image())
        off = recon.sites()[5]['offset']
        blob[off] ^= 0xAA
        result = recon.audit(blob)
        self.assertEqual(result['groups']['video_64mbps']['status'], 'unverified')
        self.assertEqual(result['groups']['video_64mbps']['unexpected'], 1)

    def test_single_site_partial_patch(self):
        blob = bytearray(fake_image())
        s = recon.sites()[0]
        blob[s['offset']:s['offset']+len(s['patched'])] = s['patched']
        result = recon.audit(blob)
        self.assertEqual(result['groups']['video_64mbps']['status'], 'mixed')

    def test_anchored_outside_data(self):
        blob = fake_image(data_size=None)
        truncated = blob[:0x1400000]  # Remove far-away video anchors
        result = recon.audit(truncated)
        self.assertEqual(result['groups']['video_64mbps']['status'], 'unverified')
        self.assertGreater(result['groups']['video_64mbps']['out_of_range'], 0)

    def test_refs_find_pointer_and_arm_bl(self):
        blob = bytearray(fake_image())
        target = BASE + 0xA0
        struct.pack_into('<I', blob, 0x40, target)
        struct.pack_into('<I', blob, 0x44, target | 1)
        # At address BASE+0x80: BL target (= PC+8+0x18)
        struct.pack_into('<I', blob, 0x80, 0xEB000006)
        refs = recon.find_refs(blob, target, mode='both', limit=20)
        self.assertIn('0x90020080', [r['rom_address'] for r in refs])
        self.assertIn('raw_pointer_arm_word', [r['kind'] for r in refs])
        self.assertIn('raw_pointer_thumb_word', [r['kind'] for r in refs])
        self.assertIn('a32_bl_candidate', [r['kind'] for r in refs])

    def test_refs_reject_unsupported_target(self):
        with self.assertRaises(ValueError):
            recon.find_refs(b'abc', 2 ** 32)
        with self.assertRaises(ValueError):
            recon.find_refs(b'abc', 0, limit=0)

    def test_elf_valid_layout_and_a32_disassembly(self):
        # Use large synthetic input because ELF exporter verifies known copy maps.
        length = max(src - BASE + l for _, src, _, l, _ in RAM_MAP) + 0x20
        blob = bytearray(fake_image(data_size=length))
        struct.pack_into('<I', blob, 0x80, 0xEB000006)  # ARM BL target at +0xA0
        out = elf.export_elf(blob)
        self.assertEqual(out[:4], b'\x7fELF')
        ehdr = struct.unpack_from('<16sHHIIIIIHHHHHH', out)
        self.assertEqual(ehdr[2], 40)  # EM_ARM
        self.assertEqual(ehdr[4], BASE + 0x80)
        self.assertEqual(out[elf.TEXT_OFFSET:elf.TEXT_OFFSET + len(blob)], blob)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'analysis.elf'
            path.write_bytes(out)
            if subprocess.run(['which', 'readelf'], stdout=subprocess.DEVNULL).returncode == 0:
                cp = subprocess.run(['readelf', '-h', str(path)], capture_output=True, text=True, check=True)
                self.assertIn('ARM', cp.stdout)
            llvmdis = Path('/usr/local/swift/usr/bin/llvm-objdump')
            if llvmdis.exists():
                cp = subprocess.run([str(llvmdis), '-d', '--start-address=0x90020080',
                    '--stop-address=0x90020084', str(path)], capture_output=True, text=True, check=True)
                self.assertIn('bl', cp.stdout.lower())

    def test_elf_cannot_export_truncated_fake(self):
        with self.assertRaises(ValueError):
            elf.export_elf(fake_image())


if __name__ == '__main__':
    unittest.main()
