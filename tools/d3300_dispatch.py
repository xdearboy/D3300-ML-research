#!/usr/bin/env python3
"""Decode Thumb TBB dispatch bytes and indexed ROM tables from a B image."""

import argparse
import json
import struct
from pathlib import Path

from d3300_lab import BASE


def _slice(data, address, size, base=BASE):
    offset = address - base
    if offset < 0 or offset + size > len(data):
        raise ValueError("address range is outside the decoded B image")
    return data[offset:offset + size]


def trace_tbb(data, address, count, indices, arrays=(), base=BASE):
    """Decode one Thumb TBB table plus optional parallel 32-bit arrays.

    Output targets are candidates for manual disassembly. This helper does not
    infer branch conditions, returned values, or runtime table contents.
    """
    if count <= 0:
        raise ValueError("count must be positive")
    instruction = _slice(data, address, 4, base)
    first, second = struct.unpack("<HH", instruction)
    if first != 0xE8DF or second & 0xFFF0 != 0xF000:
        raise ValueError("address does not contain Thumb TBB [pc, Rm]")
    table_address = address + 4
    table = _slice(data, table_address, count, base)
    result = []
    for index in indices:
        if index < 0 or index >= count:
            raise ValueError("index outside TBB table")
        item = {
            "index": index,
            "entry": table[index],
            "target": table_address + 2 * table[index],
        }
        values = {}
        for array in arrays:
            values["0x%08X" % array] = struct.unpack(
                "<I", _slice(data, array + index * 4, 4, base)
            )[0]
        if values:
            item["array_values"] = values
        result.append(item)
    return result


def _integer(value):
    return int(value, 0)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", help="decoded b860_102.bin; never an update package")
    parser.add_argument("--tbb", required=True, type=_integer, help="ROM address of TBB instruction")
    parser.add_argument("--count", required=True, type=int, help="number of byte entries")
    parser.add_argument("--index", required=True, action="append", type=_integer, help="index to decode; repeatable")
    parser.add_argument("--array", action="append", default=[], type=_integer,
                        help="optional ROM address of a parallel 32-bit array; repeatable")
    args = parser.parse_args(argv)
    data = Path(args.image).read_bytes()
    print(json.dumps(trace_tbb(data, args.tbb, args.count, args.index, args.array), indent=2))


if __name__ == "__main__":
    main()
