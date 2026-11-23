"""Tests for offline input-record decoding with synthetic records."""

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_opno import (
    decode_record,
    normalize_device_manager_state,
    queue_button_policy,
)


class OPNOTest(unittest.TestCase):
    def test_queue_record_layout(self):
        data = bytes.fromhex("40 00 02 01 78 56 34 12 F0 DE BC 9A 80 00 00 40")
        self.assertEqual(
            decode_record(data, stage="queue"),
            {
                "stage": "queue",
                "event": 0x40,
                "tag": 0x0102,
                "value_1": 0x12345678,
                "value_2": 0x9ABCDEF0,
                "mask": 0x40000080,
                "normalized_event": 8,
                "object_index": 8,
                "queue_button_filter": None,
            },
        )

    def test_queue_lookup_ranges_and_out_of_bounds(self):
        cases = (
            (0x3D, 0x3D), (0x3E, 0), (0x3F, 0), (0x40, 8), (0x47, 8),
            (0x48, 9), (0x4A, 9), (0x4B, 0x1B), (0x4D, 0x1B), (0x4E, 0x32),
            (0x4F, None), (0xFFFF, None),
        )
        for event, expected_index in cases:
            with self.subTest(event=event):
                data = struct.pack("<H", event) + bytes(14)
                decoded = decode_record(data, stage="queue")
                self.assertEqual(decoded["object_index"], expected_index)
                self.assertEqual(
                    decoded["normalized_event"],
                    event if expected_index is None else expected_index,
                )

    def test_controller_stage_preserves_received_halfword(self):
        for event in (8, 0x40, 0xFFFF):
            with self.subTest(event=event):
                data = struct.pack("<H", event) + bytes(14)
                decoded = decode_record(data, stage="controller")
                self.assertEqual(decoded["normalized_event"], event)
                self.assertEqual(decoded["object_index"], 8 if event == 8 else None)

    def test_reject_invalid_length_and_stage(self):
        for length in (0, 15, 17):
            with self.subTest(length=length), self.assertRaisesRegex(ValueError, "expected 16"):
                decode_record(bytes(length), stage="queue")
        with self.assertRaisesRegex(ValueError, "stage must be"):
            decode_record(bytes(16), stage="unknown")

    def test_verified_queue_button_filter(self):
        self.assertEqual(
            queue_button_policy(0x0B, 0),
            {"policy": "direct-copy", "accepted": True},
        )
        self.assertEqual(
            queue_button_policy(0x0C, 0),
            {"policy": "value_1-equals-1-and-runtime-guards", "accepted": False},
        )
        self.assertEqual(
            queue_button_policy(0x0C, 1),
            {"policy": "value_1-equals-1-and-runtime-guards", "accepted": None},
        )
        self.assertEqual(
            queue_button_policy(0x16, 1),
            {"policy": "outside-queue-button-range", "accepted": False},
        )

        menu = decode_record(struct.pack("<HHIII", 0x0B, 1, 0, 0, 0x100), stage="queue")
        confirm = decode_record(struct.pack("<HHIII", 0x0C, 1, 1, 0, 0x200), stage="queue")
        self.assertEqual(menu["queue_button_filter"]["accepted"], True)
        self.assertIsNone(confirm["queue_button_filter"]["accepted"])

    def test_device_manager_local_observer_translation(self):
        expected = (2, 3, 4, 10, 9, 5, 6, 11, 7, 12, 8, 0, 1)
        self.assertEqual(
            tuple(normalize_device_manager_state(event) for event in range(0x2C, 0x39)),
            expected,
        )
        self.assertEqual(normalize_device_manager_state(0x33), 0x0B)
        self.assertEqual(normalize_device_manager_state(0x35), 0x0C)
        self.assertEqual(normalize_device_manager_state(0x2B), 0)
        self.assertEqual(normalize_device_manager_state(0x39), 0)
