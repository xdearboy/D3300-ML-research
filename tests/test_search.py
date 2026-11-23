"""Tests for the persistent static-search index using synthetic bytes only."""

import sqlite3
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from d3300_lab import BASE
from d3300_search import backtrace, create_index, find_strings, get_refs


class SearchIndexTest(unittest.TestCase):
    def test_index_refs_backtrace_and_strings(self):
        blob = bytearray(0x100)
        target = BASE + 0xA0
        struct.pack_into("<I", blob, 0x20, target | 1)
        struct.pack_into("<I", blob, 0x24, BASE + 0x20)
        # T32 BL from BASE+0x80 to BASE+0xA0.
        blob[0x80:0x84] = bytes.fromhex("00f00ef8")
        name = b"CSYSCDeviceManager"
        blob[0xC0:0xC0 + len(name)] = name
        with tempfile.TemporaryDirectory() as temporary:
            database = Path(temporary) / "index.sqlite"
            metadata = create_index(bytes(blob), database)
            self.assertEqual(metadata["size"], "256")
            self.assertGreaterEqual(metadata["xref_count"], 3)
            with sqlite3.connect(database) as connection:
                refs = get_refs(connection, target)
                self.assertEqual(
                    {item["source"] for item in refs},
                    {"0x90020020", "0x90020080"},
                )
                chain = backtrace(connection, target, depth=2)
                self.assertIn(
                    (2, "0x90020024", "0x90020020"),
                    [(item["level"], item["source"], item["target"]) for item in chain],
                )
                self.assertEqual(
                    find_strings(connection, "device"),
                    [{"address": "0x900200C0", "text": "CSYSCDeviceManager"}],
                )

    def test_refuses_to_replace_index(self):
        with tempfile.TemporaryDirectory() as temporary:
            database = Path(temporary) / "index.sqlite"
            create_index(bytes(16), database)
            with self.assertRaisesRegex(ValueError, "already exists"):
                create_index(bytes(16), database)


if __name__ == "__main__":
    unittest.main()
