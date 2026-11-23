import unittest

from tools.d3300_verify_camera_status import check_anchors


class CameraStatusAnchorTest(unittest.TestCase):
    def test_exact_anchor_match_and_mismatch(self) -> None:
        anchor = {
            "name": "fixture",
            "address": 0x1004,
            "bytes": "aabbccdd",
            "observation": "test fixture",
        }
        data = b"\x00\x00\x00\x00\xaa\xbb\xcc\xdd"
        result = check_anchors(data, base=0x1000, anchors=(anchor,))[0]
        self.assertTrue(result["match"])

        damaged = bytearray(data)
        damaged[6] ^= 0xFF
        result = check_anchors(bytes(damaged), base=0x1000, anchors=(anchor,))[0]
        self.assertFalse(result["match"])

    def test_out_of_range_anchor_does_not_match(self) -> None:
        anchor = {
            "name": "outside",
            "address": 0x0FFF,
            "bytes": "00",
            "observation": "test fixture",
        }
        result = check_anchors(b"\x00", base=0x1000, anchors=(anchor,))[0]
        self.assertFalse(result["match"])


if __name__ == "__main__":
    unittest.main()
