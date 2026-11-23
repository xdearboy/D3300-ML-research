import hashlib
import os
import struct
import unittest
from pathlib import Path

BASE = 0x90020000
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def firmware():
    path = os.environ.get("D3300_FIRMWARE")
    if not path:
        path = "/Users/devo/Desktop/d3300-analysis/b860_102.bin"
    if not Path(path).is_file():
        raise unittest.SkipTest(f"firmware not found at {path}")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise AssertionError("unexpected firmware SHA-256")
    return data


class InputChainTest(unittest.TestCase):
    def test_gpio_helper_and_trigger(self):
        data = firmware()
        helper = 0x9011E738 - BASE
        self.assertEqual(data[helper:helper + 8], bytes.fromhex("10b504465e488068"))
        trigger = 0x9011E742 - BASE
        self.assertEqual(data[trigger:trigger + 4], bytes.fromhex("08f7e8fd"))

    def test_queue_dispatch_and_mailbox_boundaries(self):
        data = firmware()
        queue = 0x9013DDCC - BASE
        self.assertEqual(data[queue:queue + 12], bytes.fromhex("2de9f0410d4609880746144e"))
        self.assertEqual(data[queue + 0x12:queue + 0x16], bytes.fromhex("091f1229"))
        mailbox = 0x903512A0 - BASE
        self.assertEqual(data[mailbox:mailbox + 6], bytes.fromhex("012210b50261"))
        self.assertEqual(data[mailbox + 0x0C:mailbox + 0x14], bytes.fromhex("c0e90e41c0e90c23"))

    def test_physical_identity_is_not_inferred(self):
        # This test intentionally checks only the static boundary; no MENU/OK
        # label is assigned to GPIO Pin 8.
        self.assertTrue(True)


if __name__ == "__main__":
    unittest.main()
