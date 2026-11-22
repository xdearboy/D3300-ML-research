#!/usr/bin/env python3
"""Read one offline Nikon D3300 C 1.02 input record."""

import argparse
import json
import struct


RECORD = struct.Struct("<HHIII")
OBJECT_COUNT = 0x3E
# Jump table at 0x900FDBDE, described in reports/D3300_C102_input_gui.md.
EVENT_TO_OBJECT = (
    0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09,
    0x1B, 0x1B, 0x1B,
    0x32,
)


def decode_record(record: bytes, *, stage: str) -> dict:
    """Decode queue data or a record already returned by the controller path."""
    if len(record) != RECORD.size:
        raise ValueError(f"expected {RECORD.size} bytes, got {len(record)}")
    if stage not in ("queue", "controller"):
        raise ValueError("stage must be queue or controller")

    event, tag, value_1, value_2, mask = RECORD.unpack(record)
    normalized_event = event
    if stage == "queue" and 0x3E <= event <= 0x4E:
        normalized_event = EVENT_TO_OBJECT[event - 0x3E]

    return {
        "stage": stage,
        "event": event,
        "tag": tag,
        "value_1": value_1,
        "value_2": value_2,
        "mask": mask,
        "normalized_event": normalized_event,
        "object_index": normalized_event if normalized_event < OBJECT_COUNT else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", required=True, choices=("queue", "controller"))
    parser.add_argument("record", metavar="HEX", help="one 16-byte record in hexadecimal")
    args = parser.parse_args()

    try:
        record = decode_record(bytes.fromhex(args.record), stage=args.stage)
    except ValueError as error:
        parser.error(str(error))

    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
