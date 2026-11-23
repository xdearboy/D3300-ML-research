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
# TBB at 0x9032007A.  This is a distinct upstream application-event
# translation, not the queue's 0x3E..0x4E object-index normalization above.
APPLICATION_EVENT_TO_MANAGER = (
    0x02, 0x03, 0x04, 0x0A, 0x09, 0x05, 0x06,
    0x0B, 0x07, 0x0C, 0x08, 0x00, 0x01,
)


def normalize_application_event(event: int) -> int:
    """Apply the verified 0x90320072 translation for events 0x2C..0x38."""
    if 0x2C <= event <= 0x38:
        return APPLICATION_EVENT_TO_MANAGER[event - 0x2C]
    return 0


def queue_button_policy(event: int, value_1: int) -> dict:
    """Describe the verified COPNOQueueButton filter at 0x9013DDCC.

    The two runtime guards used for events 0x0C..0x15 cannot be evaluated from
    a saved record alone, so this deliberately reports a conditional result.
    """
    if event in (0x04, 0x05):
        accepted = value_1 == 1
        policy = "value_1-equals-1"
    elif 0x06 <= event <= 0x0B:
        accepted = True
        policy = "direct-copy"
    elif 0x0C <= event <= 0x15:
        accepted = None if value_1 == 1 else False
        policy = "value_1-equals-1-and-runtime-guards"
    else:
        accepted = False
        policy = "outside-queue-button-range"
    return {"policy": policy, "accepted": accepted}


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

    result = {
        "stage": stage,
        "event": event,
        "tag": tag,
        "value_1": value_1,
        "value_2": value_2,
        "mask": mask,
        "normalized_event": normalized_event,
        "object_index": normalized_event if normalized_event < OBJECT_COUNT else None,
    }
    # Only these two object indices are tied to COPNOQueueButton instances by
    # the audited constructor table. Do not project this class policy onto
    # other normalized objects that use different vtables.
    result["queue_button_filter"] = (
        queue_button_policy(normalized_event, value_1)
        if normalized_event in (0x0B, 0x0C) else None
    )
    return result


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
