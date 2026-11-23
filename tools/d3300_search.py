#!/usr/bin/env python3
"""Build and query a read-only static-search index for decoded D3300 B firmware.

The index contains candidate direct ARM/Thumb branches, aligned ROM pointers,
and printable strings.  It never modifies the input image and does not produce
firmware or patches.  Candidates still require disassembly at their call sites.
"""

import argparse
import hashlib
import json
import sqlite3
import struct
from pathlib import Path

from d3300_lab import BASE, load
from d3300_recon import thumb_branches


SCHEMA_VERSION = "2"
RAM_START = 0x10000000
RAM_END = 0x11000000


def a32_branches(data):
    """Yield candidate A32 immediate branches whose targets stay in the image."""
    image_end = BASE + len(data)
    for pos in range(0, len(data) - 3, 4):
        inst = struct.unpack_from("<I", data, pos)[0]
        if (inst & 0x0E000000) != 0x0A000000 or inst >> 28 == 0xF:
            continue
        delta = (inst & 0x00FFFFFF) << 2
        if delta & (1 << 25):
            delta -= 1 << 26
        target = (BASE + pos + 8 + delta) & 0xFFFFFFFF
        if BASE <= target < image_end:
            yield pos, target, "a32_bl_candidate" if inst & 0x01000000 else "a32_b_candidate", inst


def pointer_refs(data):
    """Yield aligned words resembling pointers into this ROM or known RAM."""
    image_end = BASE + len(data)
    for pos in range(0, len(data) - 3, 4):
        raw = struct.unpack_from("<I", data, pos)[0]
        target = raw & ~1
        if BASE <= target < image_end:
            space = "rom"
        elif RAM_START <= target < RAM_END:
            space = "ram"
        else:
            continue
        kind = "%s_pointer_%s_candidate" % (
            space, "thumb" if raw & 1 else "arm"
        )
        yield pos, target, kind, raw


def printable_strings(data, minimum=4):
    """Yield maximal printable ASCII strings."""
    start = None
    for pos, value in enumerate(data + b"\x00"):
        if 0x20 <= value <= 0x7E:
            if start is None:
                start = pos
        elif start is not None:
            if pos - start >= minimum:
                yield start, data[start:pos].decode("ascii")
            start = None


def create_index(data, database):
    """Create a new SQLite index; refuse to replace an existing database."""
    database = Path(database)
    if database.exists():
        raise ValueError("index already exists; choose a new path or remove it explicitly")
    connection = sqlite3.connect(str(database))
    try:
        connection.executescript(
            """
            CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
            CREATE TABLE xrefs (
                target INTEGER NOT NULL,
                source INTEGER NOT NULL,
                kind TEXT NOT NULL,
                raw INTEGER NOT NULL
            );
            CREATE INDEX xrefs_target ON xrefs(target);
            CREATE INDEX xrefs_source ON xrefs(source);
            CREATE TABLE strings (address INTEGER PRIMARY KEY, text TEXT NOT NULL);
            CREATE INDEX strings_text ON strings(text COLLATE NOCASE);
            """
        )
        metadata = {
            "schema_version": SCHEMA_VERSION,
            "image_sha256": hashlib.sha256(data).hexdigest(),
            "base": "0x%08X" % BASE,
            "size": str(len(data)),
        }
        connection.executemany("INSERT INTO meta VALUES (?, ?)", metadata.items())

        batch = []
        for pos, target, kind, raw in pointer_refs(data):
            batch.append((target, BASE + pos, kind, raw))
        for pos, target, kind, raw in a32_branches(data):
            batch.append((target, BASE + pos, kind, raw))
        for pos, target, kind, raw in thumb_branches(data):
            if BASE <= target < BASE + len(data):
                batch.append((target, BASE + pos, kind, raw))
        connection.executemany("INSERT INTO xrefs VALUES (?, ?, ?, ?)", batch)
        connection.executemany(
            "INSERT INTO strings VALUES (?, ?)",
            ((BASE + pos, text) for pos, text in printable_strings(data)),
        )
        connection.commit()
        return {
            **metadata,
            "xref_count": connection.execute("SELECT count(*) FROM xrefs").fetchone()[0],
            "string_count": connection.execute("SELECT count(*) FROM strings").fetchone()[0],
        }
    except Exception:
        connection.close()
        if database.exists():
            database.unlink()
        raise
    finally:
        try:
            connection.close()
        except Exception:
            pass


def get_refs(connection, target, limit=100):
    rows = connection.execute(
        "SELECT source, kind, raw FROM xrefs WHERE target=? ORDER BY source LIMIT ?",
        (target & ~1, limit),
    )
    return [
        {"source": "0x%08X" % source, "kind": kind, "raw": "0x%08X" % raw}
        for source, kind, raw in rows
    ]


def backtrace(connection, target, depth=2, limit=500):
    """Return breadth-first reverse references without claiming function bounds."""
    if depth < 1 or limit < 1:
        raise ValueError("depth and limit must be positive")
    seen = {target & ~1}
    frontier = [target & ~1]
    result = []
    for level in range(1, depth + 1):
        next_frontier = []
        for destination in frontier:
            rows = connection.execute(
                "SELECT source, kind, raw FROM xrefs WHERE target=? ORDER BY source",
                (destination,),
            )
            for source, kind, raw in rows:
                result.append({
                    "level": level,
                    "source": "0x%08X" % source,
                    "target": "0x%08X" % destination,
                    "kind": kind,
                    "raw": "0x%08X" % raw,
                })
                if source not in seen:
                    seen.add(source)
                    next_frontier.append(source)
                if len(result) >= limit:
                    return result
        frontier = next_frontier
        if not frontier:
            break
    return result


def find_strings(connection, query, limit=100):
    rows = connection.execute(
        "SELECT address, text FROM strings WHERE text LIKE ? COLLATE NOCASE ORDER BY address LIMIT ?",
        ("%" + query + "%", limit),
    )
    return [{"address": "0x%08X" % address, "text": text} for address, text in rows]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("index", help="Build a new SQLite index")
    build.add_argument("image", help="Decoded b860_102.bin")
    build.add_argument("database", help="New SQLite output path")
    refs = commands.add_parser("refs", help="Find direct candidate references")
    refs.add_argument("database")
    refs.add_argument("target", type=lambda value: int(value, 0))
    refs.add_argument("--limit", type=int, default=100)
    trace = commands.add_parser("backtrace", help="Walk candidate references backwards")
    trace.add_argument("database")
    trace.add_argument("target", type=lambda value: int(value, 0))
    trace.add_argument("--depth", type=int, default=2)
    trace.add_argument("--limit", type=int, default=500)
    strings = commands.add_parser("strings", help="Search indexed printable strings and RTTI")
    strings.add_argument("database")
    strings.add_argument("query")
    strings.add_argument("--limit", type=int, default=100)
    options = parser.parse_args(argv)

    try:
        if options.command == "index":
            result = create_index(load(options.image), options.database)
        else:
            connection = sqlite3.connect(options.database)
            try:
                if options.command == "refs":
                    result = get_refs(connection, options.target, options.limit)
                elif options.command == "backtrace":
                    result = backtrace(connection, options.target, options.depth, options.limit)
                else:
                    result = find_strings(connection, options.query, options.limit)
            finally:
                connection.close()
        print(json.dumps(result, indent=2, ensure_ascii=False))
    except (ValueError, OSError, sqlite3.Error) as error:
        parser.exit(2, "ERROR: %s\n" % error)


if __name__ == "__main__":
    main()
