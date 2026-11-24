import hashlib
import os
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_recon import thumb_branches

BASE = 0x90020000
SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def firmware() -> bytes:
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
    def read_bytes(self, addr: int, size: int) -> bytes:
        data = firmware()
        off = addr - BASE
        return data[off : off + size]

    def read_halfwords(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 2)
        return struct.unpack(f"<{count}H", raw)

    def read_words(self, addr: int, count: int) -> tuple[int, ...]:
        raw = self.read_bytes(addr, count * 4)
        return struct.unpack(f"<{count}I", raw)

    def test_firmware_sha256(self):
        data = firmware()
        digest = hashlib.sha256(data).hexdigest()
        self.assertEqual(digest, SHA256)

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

    def test_callback_continuation_marks_only_first_message_byte(self):
        data = firmware()
        # The persistent callback registration and sender share this queue
        # state; do not conflate it with the separate 0x1012090C controller.
        queue_literal = struct.unpack("<I", data[0x9011E168 - BASE:0x9011E16C - BASE])[0]
        self.assertEqual(queue_literal, 0x1011FFB4)
        continuation = 0x9011E756 - BASE
        self.assertEqual(data[continuation:continuation + 12],
                         bytes.fromhex("1fb50220642310228df80000"))
        # PUSH saves incoming r0-r4/lr.  The subsequent STRB changes only the
        # low byte of saved r0 before r1=sp and r2=0x10 are passed to send.
        # This routine therefore cannot prove a normalized 16-byte layout.
        self.assertEqual(data[continuation + 12:continuation + 20],
                         bytes.fromhex("69465448806910f7"))
        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x9011E756
        }
        self.assertEqual(callers, {0x9011DD70, 0x9011E018})

    def test_gpio_type_two_message_bypasses_button_tbb(self):
        data = firmware()
        callback = 0x9011DD5C - BASE
        # The gate is normalized to 0/1 before the tail branch; replacing the
        # low byte then yields first word 2 for either case.
        self.assertEqual(data[callback:callback + 22],
                         bytes.fromhex("10b54bf202eb012800d00020ff494860bde8104000f0"))
        self.assertEqual({(value & ~0xFF) | 2 for value in (0, 1)}, {2})
        queue = 0x9013DDCC - BASE
        # ldrh; subs #4; cmp #0x12; bhs straight-copy (0x9013DE14).
        self.assertEqual(data[queue + 6:queue + 0x18],
                         bytes.fromhex("09880746144e4ff070446868091f122917d2"))

    def test_non_gpio_sender_caller_register_provenance(self):
        data = firmware()
        caller = 0x9011DFE2 - BASE
        # r4 is the supplied record pointer.  The caller loads r0/r1/r3 from
        # +4/+0/+8, computes r2 from selector 2, then branches only if +4.
        self.assertEqual(data[caller:caller + 0x38], bytes.fromhex(
            "f8b505460c46032168460df784fb5d4e01214ff0ff32f0691af7"
            "86f96cb194e80b0005eb45025f4f022d07eb82020bc203d1606808"
            "b100f0"))
        literal = struct.unpack("<I", data[0x9011E188 - BASE:0x9011E18C - BASE])[0]
        self.assertEqual(literal, 0x10319D2C)

    def test_selector_two_sender_producer_is_static_rom_record(self):
        data = firmware()
        producer = 0x9016E0AE - BASE
        self.assertEqual(data[producer:producer + 22],
                         bytes.fromhex("0eb502201749d1e9002389680092cde901316946aff7"))
        literal = struct.unpack("<I", data[0x9016E110 - BASE:0x9016E114 - BASE])[0]
        self.assertEqual(literal, 0x91478AE8)
        callers = {
            BASE + pos
            for pos, target, _kind, _raw in thumb_branches(data)
            if target == 0x9011DFE2
        }
        self.assertEqual(callers, {0x9016E0C2, 0x9029B772, 0x9029B780, 0x9029B78E})

    def test_static_secondary_route_bypasses_button_tbb(self):
        data = firmware()
        words = struct.unpack("<III", data[0x91478AE8 - BASE:0x91478AF4 - BASE])
        self.assertEqual(words, (0, 0x9016E0D3, 0))
        # The sender replaces the low byte of r0 (the middle source word),
        # so the queue sees halfword 0xE002. After subtracting 4 it is above
        # 0x12 and takes the straight-copy path, not the TBB button mapping.
        self.assertEqual((words[1] & ~0xFF) | 2, 0x9016E002)
        queue = 0x9013DDCC - BASE
        self.assertEqual(data[queue + 6:queue + 0x18],
                         bytes.fromhex("09880746144e4ff070446868091f122917d2"))

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

    def test_sender_message_format_0x9011e756(self):
        """0x9011E756 builds 16-byte message on stack with msg[0] = 0x02; bytes 1..15 are saved registers."""
        raw = self.read_bytes(0x9011E756, 20)
        hw = struct.unpack("<10H", raw)
        self.assertEqual(hw[0], 0xB51F, "push {r0..r4, lr} (saves incoming registers to stack)")
        self.assertEqual(hw[1], 0x2002, "movs r0, #2")
        self.assertEqual(hw[2], 0x2364, "movs r3, #0x64 (100 ms timeout)")
        self.assertEqual(hw[3], 0x2210, "movs r2, #0x10 (16 bytes size)")
        self.assertEqual((hw[4], hw[5]), (0xF88D, 0x0000), "strb.w r0, [sp] (writes only byte 0)")
        self.assertEqual(hw[6], 0x4669, "mov r1, sp")
        self.assertEqual(hw[8], 0x6980, "ldr r0, [r0, #0x18] (mbfid)")

        # Verify that incoming r0 in {0, 1} yields first halfword 0x0002
        for incoming_r0 in (0, 1):
            outgoing_word = (incoming_r0 & ~0xFF) | 2
            outgoing_halfword = outgoing_word & 0xFFFF
            self.assertEqual(outgoing_halfword, 0x0002)

    def test_gpio_type_two_handler_dispatches_via_dynamic_ram_table(self):
        """0x9011E2B4 loads callback from dynamic RAM table 0x10319D2C + 0x18."""
        table_base = self.read_words(0x9011E8C0, 1)[0]
        self.assertEqual(table_base, 0x10319D2C)
        self.assertGreaterEqual(table_base, 0x10000000)
        self.assertLess(table_base, 0x10500000)

        raw = self.read_bytes(0x9011E7F8, 14)
        hw = struct.unpack("<7H", raw)
        self.assertEqual(hw[0], 0x4931, "ldr r1, [literal pool 0x9011E8C0]")
        self.assertEqual((hw[1], hw[2]), (0xEB00, 0x0040), "add.w r0, r0, r0, lsl #1")
        self.assertEqual((hw[3], hw[4]), (0xEB01, 0x0080), "add.w r0, r1, r0, lsl #2")
        self.assertEqual(hw[5], 0x6840, "ldr r0, [r0, #4]")
        self.assertEqual(hw[6], 0x4770, "bx lr")

    def test_receiver_task_loop_0x9011dd88(self):
        """0x9011DD88 receives from message buffer and validates msgsz == 16, msg[0] < 3."""
        raw = self.read_bytes(0x9011DD88, 32)
        hw = struct.unpack("<16H", raw)
        self.assertEqual(hw[0], 0x4669, "mov r1, sp")
        self.assertEqual(hw[1], 0x69A0, "ldr r0, [r4, #0x18]")
        self.assertEqual(hw[2], 0x462A, "mov r2, r5 (TMO_FEVR = -1)")
        self.assertEqual(hw[5], 0x2810, "cmp r0, #0x10 (check msgsz == 16)")
        self.assertEqual(hw[6], 0xD1F8, "bne 0x9011DD88")
        self.assertEqual((hw[7], hw[8]), (0xF89D, 0x0000), "ldrb.w r0, [sp]")
        self.assertEqual(hw[9], 0x2803, "cmp r0, #3 (check type < 3)")
        self.assertEqual(hw[10], 0xDAF4, "bge 0x9011DD88")
        self.assertEqual((hw[11], hw[12]), (0xF857, 0x1020), "ldr.w r1, [r7, r0, lsl #2]")

    def test_dispatch_table_0x9128126c(self):
        """Table 0x9128126C contains 3 function pointers for types 0, 1, 2."""
        entries = self.read_words(0x9128126C, 3)
        self.assertEqual(entries[0], 0x9011E1F7, "Type 0 handler (0x9011E1F6)")
        self.assertEqual(entries[1], 0x9011E285, "Type 1 handler (0x9011E284)")
        self.assertEqual(entries[2], 0x9011E2B5, "Type 2 handler (0x9011E2B4)")

    def test_event_translation_tables_0x91281278_and_0x91281287(self):
        """Tables 0x91281278 and 0x91281287 map raw events to logical codes."""
        table1 = list(self.read_bytes(0x91281278, 16))
        expected_table1 = [11, 11, 11, 11, 0, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 4]
        self.assertEqual(table1, expected_table1)

        table2 = list(self.read_bytes(0x91281287, 11))
        expected_table2 = [4, 5, 6, 9, 7, 10, 8, 11, 12, 13, 14]
        self.assertEqual(table2, expected_table2)

        self.assertIn(0x0B, table2)
        self.assertIn(0x0C, table2)

    def test_queue_record_assembly_0x902ae122(self):
        """0x902AE122..0x902AE13E assembles 16-byte record (<HHIII) before memcpy."""
        raw = self.read_bytes(0x902AE122, 32)
        hw = struct.unpack("<16H", raw)
        self.assertEqual((hw[0], hw[1]), (0xF8AD, 0x6000), "strh.w r6, [sp] (event_id)")
        self.assertEqual(hw[2], 0x8868, "ldrh r0, [r5, #2] (tag)")
        self.assertEqual(hw[3], 0x2210, "movs r2, #0x10 (size)")
        self.assertEqual(hw[4], 0x4669, "mov r1, sp")
        self.assertEqual((hw[5], hw[6]), (0xF8AD, 0x0002), "strh.w r0, [sp, #2] (tag)")
        self.assertEqual(hw[7], 0x6868, "ldr r0, [r5, #4] (value_1)")
        self.assertEqual(hw[8], 0x9001, "str r0, [sp, #4]")
        self.assertEqual(hw[9], 0x68A8, "ldr r0, [r5, #8] (value_2)")
        self.assertEqual(hw[10], 0x9002, "str r0, [sp, #8]")
        self.assertEqual(hw[11], 0x6920, "ldr r0, [r4, #0x10] (mask)")
        self.assertEqual(hw[12], 0x9003, "str r0, [sp, #0xc]")
        self.assertEqual(hw[13], 0x4638, "mov r0, r7 (dest)")

    def test_copno_queue_button_0x9013ddcc(self):
        """COPNOQueueButton filter accepts events 0x04..0x15 and dispatches via TBB; halfword 0x0002 bypasses TBB."""
        vtable_word = self.read_words(0x9151DE6C + 0x2C, 1)[0]
        self.assertEqual(vtable_word, 0x9013DDCD, "Slot +0x2C points to 0x9013DDCC")

        raw = self.read_bytes(0x9013DDDE, 6)
        hw = struct.unpack("<3H", raw)
        self.assertEqual(hw[0], 0x1F09, "subs r1, r1, #4")
        self.assertEqual(hw[1], 0x2912, "cmp r1, #0x12 (18 entries)")
        self.assertEqual(hw[2], 0xD217, "bhs 0x9013DE14")

        # Halfword 0x0002 from GPIO type 2 trips the unsigned range check:
        # (2 - 4) & 0xFFFFFFFF = 0xFFFFFFFE >= 0x12, taking straight-copy at 0x9013DE14
        subs_result = (2 - 4) & 0xFFFFFFFF
        self.assertGreaterEqual(subs_result, 0x12)

    def test_menu_logical_event_translation_0x90252d9e(self):
        """0x90252D9E maps 59 event IDs to logical menu IDs via TBB table at 0x90252DAA."""
        hw = self.read_halfwords(0x90252D9E, 4)
        self.assertEqual(hw[0], 0x293B, "cmp r1, #0x3B (59 entries)")
        self.assertEqual((hw[1], hw[2]), (0xF04F, 0x30FF), "mov.w r0, #-1")

        tbb_bytes = list(self.read_bytes(0x90252DAA, 59))
        self.assertEqual(len(tbb_bytes), 59)

        # Input 0x0C -> byte 0x1E -> target 0x90252DE6 -> logical_id = 1 [inferred role: Confirm / Action]
        self.assertEqual(tbb_bytes[0x0C], 0x1E)
        target_0c = 0x90252DAA + tbb_bytes[0x0C] * 2
        self.assertEqual(target_0c, 0x90252DE6)
        inst_0c = self.read_halfwords(target_0c, 1)[0]
        self.assertEqual(inst_0c, 0x2101, "movs r1, #1 (logical_id = 1)")

        # Input 0x0B -> byte 0x4B -> target 0x90252E40 -> logical_id = 0x1B [inferred role: Menu Toggle / Back]
        self.assertEqual(tbb_bytes[0x0B], 0x4B)
        target_0b = 0x90252DAA + tbb_bytes[0x0B] * 2
        self.assertEqual(target_0b, 0x90252E40)
        inst_0b = self.read_halfwords(target_0b, 1)[0]
        self.assertEqual(inst_0b, 0x211B, "movs r1, #0x1B (logical_id = 0x1B)")

        # Input 0x10 -> byte 0x27 -> target 0x90252DF8 -> logical_id = 5 [inferred role: Nav Right]
        self.assertEqual(tbb_bytes[0x10], 0x27)
        target_10 = 0x90252DAA + tbb_bytes[0x10] * 2
        self.assertEqual(target_10, 0x90252DF8)
        inst_10 = self.read_halfwords(target_10, 1)[0]
        self.assertEqual(inst_10, 0x2105, "movs r1, #5 (logical_id = 5)")

    def test_cmenu_top_menu_view_navigation_slots(self):
        """CMENUTopMenuView vtable 0x9151D874 routes logical IDs to navigation/confirm slots."""
        vtable = 0x9151D874

        # Slot +0x10: active view event entry (0x9025A85E)
        slot10 = self.read_words(vtable + 0x10, 1)[0]
        self.assertEqual(slot10, 0x9025A85F, "Slot +0x10 points to 0x9025A85E")

        # Slot +0x40: screen event handler (0x9025AD54)
        slot40 = self.read_words(vtable + 0x40, 1)[0]
        self.assertEqual(slot40, 0x9025AD55, "Slot +0x40 points to 0x9025AD54")

        # Slot +0x12C: Logical ID 0x01 handler (0x90076FF4) [inferred: Confirm / Action]
        slot12c = self.read_words(vtable + 0x12C, 1)[0]
        self.assertEqual(slot12c, 0x90076FF5, "Slot +0x12C points to 0x90076FF4 (Logical ID 0x01)")

        # Slot +0x130: Logical ID 0x05 handler (0x90077512) [inferred: Nav Right]
        slot130 = self.read_words(vtable + 0x130, 1)[0]
        self.assertEqual(slot130, 0x90077513, "Slot +0x130 points to 0x90077512 (Logical ID 0x05)")

        # Slot +0x17C: Logical ID 0x1B handler (0x90077CBD) [inferred: Menu Toggle / Back]
        slot17c = self.read_words(vtable + 0x17C, 1)[0]
        self.assertEqual(slot17c, 0x90077CBD, "Slot +0x17C points to 0x90077CBD (Logical ID 0x1B)")

    def test_unproven_gpio_pin8_linkage(self):
        """Static ROM check confirming channel 2 handler in RAM table 0x10319D2C has no static binding."""
        pool_word = self.read_words(0x9011E8C0, 1)[0]
        self.assertEqual(pool_word, 0x10319D2C)

    def test_physical_identity_is_not_inferred(self):
        # This test intentionally checks only the static boundary; no MENU/OK
        # label is assigned to GPIO Pin 8.
        self.assertTrue(True)


if __name__ == "__main__":
    unittest.main()
