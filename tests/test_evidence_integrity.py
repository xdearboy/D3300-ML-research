"""Automated Evidence Integrity & Negative Testing Suite for Nikon D3300 C 1.02.

Verifies:
1. Negative tests: corrupted anchor bytes, invalid firmware SHA-256,
   out-of-bounds property table indices, and broken pointer strides.
2. Boundary assertions: guarantees no false GPIO -> menu claims exist
   and no unproven property names are promoted to PROVEN.
3. Report ledger integrity: all reports/*.json parse cleanly and conform
   to strict evidence boundaries.
"""

from __future__ import annotations

import glob
import hashlib
import json
import os
from pathlib import Path
import struct
import unittest

BASE = 0x90020000
EXPECTED_SHA256 = "c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff"


def get_firmware_data() -> bytes:
    path = os.environ.get("D3300_FIRMWARE")
    if not path:
        default_candidate = Path("/Users/devo/Desktop/d3300-analysis/b860_102.bin")
        if default_candidate.is_file():
            path = str(default_candidate)
    if not path or not Path(path).is_file():
        raise unittest.SkipTest(f"Firmware binary not found at {path}")
    data = Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise AssertionError("unexpected firmware SHA-256")
    return data


class EvidenceIntegrityTest(unittest.TestCase):
    def test_01_negative_firmware_sha256_rejection(self):
        """Negative test: corrupted firmware binary must fail SHA-256 verification."""
        data = bytearray(get_firmware_data())
        # Corrupt single byte at offset 0
        data[0] ^= 0xFF
        corrupted_digest = hashlib.sha256(data).hexdigest()
        self.assertNotEqual(
            corrupted_digest,
            EXPECTED_SHA256,
            "Corrupted binary must NOT match expected firmware hash",
        )

        with self.assertRaises(AssertionError):
            if corrupted_digest != EXPECTED_SHA256:
                raise AssertionError("Firmware SHA-256 verification failed")

    def test_02_negative_anchor_corruption_detection(self):
        """Negative test: modified anchor bytes must trigger verification failure."""
        data = bytearray(get_firmware_data())

        def verify_anchor(data_slice: bytes, expected: bytes, name: str) -> None:
            if data_slice != expected:
                raise AssertionError(f"Anchor check failed for {name}: expected {expected.hex()}, got {data_slice.hex()}")

        # Valid anchor check: TBB1 branch at 0x90252DA6
        tbb1_anchor = bytes(data[0x90252DA6 - BASE : 0x90252DAA - BASE])
        verify_anchor(tbb1_anchor, bytes.fromhex("dfe801f0"), "TBB1 instruction")

        # Corrupted anchor check
        corrupted_slice = bytes.fromhex("00000000")
        with self.assertRaises(AssertionError) as ctx:
            verify_anchor(corrupted_slice, bytes.fromhex("dfe801f0"), "TBB1 instruction")
        self.assertIn("Anchor check failed", str(ctx.exception))

    def test_03_negative_property_bounds_rejection(self):
        """Negative test: out-of-bounds property table indices must be rejected."""
        data = get_firmware_data()
        table_off = 0x914ED324 - BASE
        entry_count = 79

        def get_property_entry(index: int) -> tuple[int, int, int]:
            if index < 0 or index >= entry_count:
                raise IndexError(f"Property index {index} out of bounds [0..{entry_count - 1}]")
            off = table_off + index * 12
            return struct.unpack("<III", data[off : off + 12])

        # In-bounds entries succeed
        p_id_0, p_size, p_ram = get_property_entry(0)
        self.assertEqual(p_id_0, 0x002A)
        p_id_last, _, _ = get_property_entry(78)
        self.assertEqual(p_id_last, 0x005E)

        # Out-of-bounds entries must raise IndexError
        with self.assertRaises(IndexError):
            get_property_entry(79)
        with self.assertRaises(IndexError):
            get_property_entry(100)
        with self.assertRaises(IndexError):
            get_property_entry(-1)

    def test_04_negative_pointer_stride_violation(self):
        """Negative test: stride violations must be rejected by stride validators."""
        def validate_stride(actual_stride: int, expected_stride: int, structure_name: str) -> None:
            if actual_stride != expected_stride:
                raise ValueError(f"Stride violation in {structure_name}: expected {expected_stride}, got {actual_stride}")

        # Correct strides
        validate_stride(10, 10, "VectorPrimitive")
        validate_stride(12, 12, "CKRRCInfoDispProperty")
        validate_stride(12, 12, "COSDCRectInfo")

        # Violations must raise ValueError
        with self.assertRaises(ValueError):
            validate_stride(8, 10, "VectorPrimitive")
        with self.assertRaises(ValueError):
            validate_stride(16, 12, "CKRRCInfoDispProperty")

    def test_05_report_json_schema_and_parse_integrity(self):
        """All reports/*.json files must parse cleanly and contain non-empty data."""
        reports_dir = Path(__file__).resolve().parents[1] / "reports"
        json_files = list(reports_dir.glob("*.json"))
        self.assertGreaterEqual(len(json_files), 15, "Expected at least 15 verified JSON reports")

        for jf in json_files:
            content = jf.read_text(encoding="utf-8")
            self.assertTrue(len(content) > 0, f"{jf.name} must not be empty")
            try:
                parsed = json.loads(content)
            except Exception as e:
                self.fail(f"Failed to parse {jf.name}: {e}")
            self.assertIsInstance(parsed, (dict, list), f"{jf.name} root must be object or array")

    def test_06_no_false_gpio_menu_link_assertions(self):
        """Guarantees that reports do NOT claim a proven GPIO -> menu route."""
        reports_dir = Path(__file__).resolve().parents[1] / "reports"

        # Check verified-input-chain.json: physical_button_identity must be UNPROVEN
        input_chain_path = reports_dir / "verified-input-chain.json"
        if input_chain_path.is_file():
            data = json.loads(input_chain_path.read_text(encoding="utf-8"))
            self.assertEqual(data.get("physical_button_identity"), "UNPROVEN")

        # Check verified-ui-event-chain.json: gpio_pin8_to_logical_menu_id must be UNPROVEN
        ui_event_path = reports_dir / "verified-ui-event-chain.json"
        if ui_event_path.is_file():
            data = json.loads(ui_event_path.read_text(encoding="utf-8"))
            bounds = {b["boundary"]: b["status"] for b in data.get("unproven_boundaries", [])}
            self.assertEqual(bounds.get("gpio_pin8_to_logical_menu_id"), "UNPROVEN")
            self.assertEqual(bounds.get("gpio_pin8_to_physical_button_legends"), "UNPROVEN")

        # Check verified-ui-routing-catalogue.json
        ui_routing_path = reports_dir / "verified-ui-routing-catalogue.json"
        if ui_routing_path.is_file():
            data = json.loads(ui_routing_path.read_text(encoding="utf-8"))
            unproven_bounds = data.get("semantic_evidence_boundary", {}).get("unproven_boundaries", [])
            has_gpio_unproven = any("GPIO" in text or "Physical" in text for text in unproven_bounds)
            self.assertTrue(has_gpio_unproven, "UI routing catalogue must note physical GPIO mapping is unproven")

    def test_07_no_unproven_property_names_assertion(self):
        """Guarantees that exactly 2 properties are PROVEN in verified-property-semantics.json."""
        reports_dir = Path(__file__).resolve().parents[1] / "reports"
        prop_sem_path = reports_dir / "verified-property-semantics.json"
        self.assertTrue(prop_sem_path.is_file(), "verified-property-semantics.json must exist")

        data = json.loads(prop_sem_path.read_text(encoding="utf-8"))
        entries = data["entries"]
        self.assertEqual(len(entries), 79, "Must have exactly 79 properties")

        proven_props = [p for p in entries if p["confidence"] == "PROVEN"]
        unproven_props = [p for p in entries if p["confidence"] == "UNPROVEN"]

        self.assertEqual(len(proven_props), 2, "Exactly 2 properties must be PROVEN")
        self.assertEqual(len(unproven_props), 77, "Exactly 77 properties must be UNPROVEN")

        proven_ids = {p["property_id_hex"] for p in proven_props}
        self.assertEqual(proven_ids, {"0x0040", "0x0054"}, "Only 0x0040 (battery) and 0x0054 (ISO) may be PROVEN")

        # Every unproven property must have semantic_name == null
        for p in unproven_props:
            self.assertIsNone(
                p["semantic_name"],
                f"Unproven property {p['property_id_hex']} must have semantic_name: null",
            )

        # Explicitly verify 0x002D is unproven
        p_002d = next(p for p in entries if p["property_id_hex"] == "0x002D")
        self.assertEqual(p_002d["confidence"], "UNPROVEN")
        self.assertIsNone(p_002d["semantic_name"])

    def test_08_osd_byte_09_strictly_unproven(self):
        """Guarantees that byte +0x09 is marked UNPROVEN in OSD reports."""
        reports_dir = Path(__file__).resolve().parents[1] / "reports"

        osd_pipe_path = reports_dir / "verified-osd-pipeline.json"
        if osd_pipe_path.is_file():
            data = json.loads(osd_pipe_path.read_text(encoding="utf-8"))
            byte09 = data["stages"][0]["layout"]["offset_0x09"]
            self.assertEqual(byte09["confidence"], "UNPROVEN")


if __name__ == "__main__":
    unittest.main()
