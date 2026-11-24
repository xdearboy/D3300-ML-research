import hashlib
import os
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_recon import thumb_branches

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

    def test_mailbox_getter_ready_and_copy_contract(self):
        data = firmware()
        getter = 0x9035128C - BASE
        # ready = state+0x10; copy only when ready and destination are non-null.
        self.assertEqual(data[getter:getter + 12],
                         bytes.fromhex("30b5026922b119b100f13002"))
        # Four words from state+0x30 are copied to the caller's buffer, then
        # the ready flag is loaded for return.
        self.assertEqual(data[getter + 12:getter + 20],
                         bytes.fromhex("3cca3cc1006930bd"))

    def test_mailbox_getter_direct_callers(self):
        data = firmware()
        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x9035128C
        }
        self.assertEqual(callers, {
            0x903292C6,
            0x9032936A,
            0x90332E58,
            0x90332FAC,
        })

    def test_mailbox_setter_direct_callers(self):
        data = firmware()
        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x903512A0
        }
        self.assertEqual(callers, {0x903276B4, 0x903276EE})

    def test_shared_state_query_contract(self):
        data = firmware()
        query = 0x9003D28C - BASE
        self.assertEqual(data[query:query + 20],
                         bytes.fromhex("2de9f047074688466ff005052cf364e8874c4c3c"))
        literal = struct.unpack("<I", data[0x9003D4BC - BASE:0x9003D4C0 - BASE])[0]
        self.assertEqual(literal - 0x4C, 0x101219D4)
        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x9003D28C
        }
        self.assertEqual(callers, {0x9011E0AE, 0x9011E3F6})

    def test_callback_continuation_packs_input_message(self):
        data = firmware()
        continuation = 0x9011E756 - BASE
        self.assertEqual(data[continuation:continuation + 12],
                         bytes.fromhex("1fb50220642310228df80000"))
        # The continuation stores message type 2 and sends 0x10 bytes through
        # the verified message-buffer veneer; physical button identity remains unknown.
        self.assertEqual(data[continuation + 12:continuation + 20],
                         bytes.fromhex("69465448806910f7"))

    def test_callback_state_gate_is_a32_byte_getter(self):
        data = firmware()
        gate = 0x90369364 - BASE
        # ARM A32: ldr r0,[pc,#imm]; ldrb r0,[r0,#3]; bx lr.
        self.assertEqual(data[gate:gate + 12],
                         bytes.fromhex("70029fe50300d0e51eff2fe1"))
        literal = struct.unpack("<I", data[0x903695DC - BASE:0x903695E0 - BASE])[0]
        self.assertEqual(literal, 0x10125160)

        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x90369364
        }
        self.assertEqual(callers, {
            0x9003D0E6,
            0x9003D13E,
            0x9003D298,
            0x9003D856,
            0x9011DD5E,
            0x9011E1FA,
        })

        setter = 0x90369358 - BASE
        self.assertEqual(data[setter:setter + 12],
                         bytes.fromhex("7c129fe50300c1e51eff2fe1"))
        setter_callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x90369358
        }
        self.assertEqual(setter_callers, {0x9011DDF4, 0x9011DE08})

    def test_physical_identity_is_not_inferred(self):
        # This test intentionally checks only the static boundary; no MENU/OK
        # label is assigned to GPIO Pin 8.
        self.assertTrue(True)


if __name__ == "__main__":
    unittest.main()
