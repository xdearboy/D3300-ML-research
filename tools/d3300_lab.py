#!/usr/bin/env python3
"""Read-only Nikon D3300 B-firmware reconnaissance (standard library only).

Input MUST be extracted B-firmware (b860_102.bin), not the Nikon .bin update
container. This tool intentionally cannot create flashable firmware.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

BASE = 0x90020000
RESET_INSTRUCTION = bytes.fromhex('18 f0 9f e5')  # ldr pc, [pc, #0x18]
# FirmwareNotes.md, D3300 B firmware C 1.02. Source values are ROM addresses,
# not offsets in the update package.
RAM_MAP = (
    ('kernel_text', 0x9158A358, 0x10000000, 0x0003B5B4, 'rwx'),
    ('kernel_data', 0x915C590C, 0x1003B5B4, 0x0000179C, 'rw'),
    ('kernel_data_2', 0x916A73A0, 0x1011D420, 0x00028B3C, 'rw'),
    ('kernel_data_3', 0x916A739C, 0x1011D03C, 4, 'rw'),
)


def load(path):
    path = Path(path)
    if not path.is_file():
        raise ValueError('Файл не найден: {}'.format(path))
    data = path.read_bytes()
    if len(data) < 0x100:
        raise ValueError('Файл слишком короткий для B-firmware')
    return data


def analyze(data):
    if data[:4] != RESET_INSTRUCTION:
        raise ValueError('Нет ожидаемого ARM reset vector. Нужен ДЕКОДИРОВАННЫЙ B-firmware b860_102.bin, а не пакет D3300_0102.bin')
    reset_target = int.from_bytes(data[0x20:0x24], 'little')
    end = BASE + len(data)
    if not BASE <= reset_target < end:
        raise ValueError('Некорректный reset pointer 0x{:08X}; убедись, что это D3300 B-firmware'.format(reset_target))
    segments = []
    for name, src, dst, length, flags in RAM_MAP:
        offset = src - BASE
        fits = offset >= 0 and offset + length <= len(data)
        segments.append(dict(name=name, source=f'0x{src:08X}', destination=f'0x{dst:08X}',
                             file_offset=f'0x{offset:X}', length=length,
                             permissions=flags, available=fits))
    return dict(image_kind='decoded Nikon D3300 B-firmware (heuristic)',
                architecture='ARMv7 little-endian', base=f'0x{BASE:08X}',
                size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                reset_vector=f'0x{reset_target:08X}',
                reset_vector_file_offset=f'0x{reset_target-BASE:X}',
                ram_maps=segments, all_maps_in_bounds=all(s['available'] for s in segments),
                caution='Reset check is not a cryptographic camera/model/version authentication.')


def human_report(result):
    print('Image: {}\nARM base: {} / {}\nSize: {} bytes\nSHA256: {}\nReset -> {}'.format(
        result['image_kind'], result['base'], result['architecture'], result['size'],
        result['sha256'], result['reset_vector']))
    print('Known RAM copy maps (D3300 C 1.02):')
    for s in result['ram_maps']:
        print('  {:16s} {} -> {} +0x{:X} [{}] {}'.format(s['name'], s['source'],
            s['destination'], s['length'], s['permissions'],
            'OK' if s['available'] else 'OUTSIDE IMAGE'))
    if not result['all_maps_in_bounds']:
        print('WARNING: Not all documented RAM source ranges fit. Wrong version/image?', file=sys.stderr)
    print('Note: the reset header alone does not certify firmware identity/version.')


def cmd_inspect(args):
    data = load(args.image)
    result = analyze(data)
    human_report(result)
    if args.json:
        Path(args.json).write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        print('Wrote:', args.json)


def cmd_ram(args):
    data = load(args.image)
    result = analyze(data)
    if not result['all_maps_in_bounds']:
        raise ValueError('Часть известной карты RAM выходит за границы прошивки; отказ от экспорта')
    dst = Path(args.output)
    dst.mkdir(parents=True, exist_ok=True)
    for name, src, addr, length, flags in RAM_MAP:
        start = src - BASE
        file = dst / ('{}_0x{:08X}.bin'.format(name, addr))
        if file.exists() and not args.force:
            raise ValueError('Файл уже существует: {}. Для перезаписи нужен --force'.format(file))
    for name, src, addr, length, flags in RAM_MAP:
        start = src - BASE
        file = dst / ('{}_0x{:08X}.bin'.format(name, addr))
        file.write_bytes(data[start:start + length])
        print('Wrote:', file)
    (dst / 'manifest.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def cmd_find(args):
    data = load(args.image)
    analyze(data)
    pattern = bytes.fromhex(args.hexbytes.replace('0x','').replace(',', ' '))
    if not pattern:
        raise ValueError('Пустой hex pattern')
    offset = 0
    count = 0
    while True:
        idx = data.find(pattern, offset)
        if idx < 0:
            break
        print('offset=0x{:X} address=0x{:08X} bytes={}'.format(idx, BASE+idx, pattern.hex(' ')))
        count += 1
        if count >= args.limit:
            print('Limit reached; possibly more matches')
            break
        offset = idx + 1
    if not count:
        print('No matches')


def cmd_strings(args):
    data = load(args.image)
    analyze(data)
    minimum = args.minimum
    if not 3 <= minimum <= 100:
        raise ValueError('minimum length must be 3..100')
    ascii_pattern = re.compile(rb'[\x20-\x7e]{%d,}' % minimum)
    utf16_pattern = re.compile((rb'(?:[\x20-\x7e]\x00){%d,}' % minimum))
    needle = args.contains.lower() if args.contains else None
    results = []
    for mode, pattern in (('ascii', ascii_pattern), ('utf16le', utf16_pattern)):
        for match in pattern.finditer(data):
            raw = match.group()
            s = raw.decode('ascii' if mode == 'ascii' else 'utf-16le', errors='replace')
            if needle is not None and needle not in s.lower():
                continue
            results.append((match.start(), mode, s))
    results.sort(key=lambda x: x[0])
    for idx, mode, s in results[:args.limit]:
        print('0x{:08X} {} {}'.format(BASE + idx, mode, s[:180]))
    print('Found {} matching strings, shown {}'.format(len(results), min(len(results), args.limit)))


def cmd_diff(args):
    a, b = load(args.first), load(args.second)
    analyze(a)
    analyze(b)
    print('A: {} bytes sha256={}'.format(len(a), hashlib.sha256(a).hexdigest()))
    print('B: {} bytes sha256={}'.format(len(b), hashlib.sha256(b).hexdigest()))
    n = min(len(a), len(b))
    runs = []
    start = None
    for i in range(n):
        different = a[i] != b[i]
        if different and start is None:
            start = i
        elif not different and start is not None:
            runs.append((start, i))
            start = None
    if start is not None:
        runs.append((start, n))
    if len(a) != len(b):
        runs.append((n, max(len(a), len(b))))
    print('Different contiguous ranges:', len(runs))
    for start, end in runs[:args.limit]:
        print('offset 0x{:X}..0x{:X}, ROM 0x{:08X}, len 0x{:X}'.format(
            start, end-1, BASE+start, end-start))
        print('  A:', a[start:min(end,start+16)].hex(' '))
        print('  B:', b[start:min(end,start+16)].hex(' '))
    if len(runs) > args.limit:
        print('... {} more ranges (increase --limit)'.format(len(runs)-args.limit))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('inspect', help='Read-only identity checks and D3300 memory map')
    p.add_argument('image')
    p.add_argument('--json', help='Write JSON report')
    p.set_defaults(func=cmd_inspect)
    p = commands.add_parser('extract-ram', help='Extract known ROM-to-RAM source regions (NOT full firmware unpack)')
    p.add_argument('image')
    p.add_argument('-o', '--output', required=True)
    p.add_argument('--force', action='store_true')
    p.set_defaults(func=cmd_ram)
    p = commands.add_parser('find-bytes', help='Find an exact hex byte sequence in decoded B-firmware')
    p.add_argument('image')
    p.add_argument('hexbytes', help='e.g. "18 f0 9f e5"')
    p.add_argument('--limit', type=int, default=30)
    p.set_defaults(func=cmd_find)
    p = commands.add_parser('strings', help='Search ASCII and UTF-16LE strings')
    p.add_argument('image')
    p.add_argument('--contains', help='Optional case-insensitive substring filter')
    p.add_argument('--minimum', type=int, default=6)
    p.add_argument('--limit', type=int, default=100)
    p.set_defaults(func=cmd_strings)
    p = commands.add_parser('diff', help='Compare two decoded B-firmware images')
    p.add_argument('first')
    p.add_argument('second')
    p.add_argument('--limit', type=int, default=30)
    p.set_defaults(func=cmd_diff)
    args = parser.parse_args(argv)
    if hasattr(args,'limit') and args.limit < 1:
        parser.error('--limit должен быть > 0')
    try:
        args.func(args)
    except (ValueError, OSError) as exc:
        parser.exit(2, 'ERROR: {}\n'.format(exc))

if __name__ == '__main__':
    main()
