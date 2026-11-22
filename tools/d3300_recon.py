#!/usr/bin/env python3
"""Read-only cross-reference and Nikon Hacker patch-anchor audit for D3300 B C1.02.

No firmware modification or patch output is implemented. This is a research
utility; a positive audit does not establish that running arbitrary code is safe.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

from d3300_lab import BASE, analyze, load

PATCH_SOURCE = 'https://github.com/simeonpilgrim/nikon-firmware-tools/blob/main/Nikon-Patch-JS/src/nikon_patch/patches.c'

# Site offsets refer to DECODED B firmware (block 1), NOT Nikon update-container
# offsets. Extracted from the D3300_0102_* entries in Nikon Hacker patches.c.
_VIDEO_A = (0x143C8BC, 0x143C8CC, 0x143C8D4, 0x143C8E4,
            0x143C8F4, 0x143C904, 0x143C914)
_VIDEO_B = (0x143C2C4, 0x143C2DC, 0x143C2F4, 0x143C30C,
            0x143C324, 0x143C348, 0x143C360, 0x143C36C,
            0x143C384, 0x143C39C, 0x143C3B4, 0x143C3CC, 0x143C414)
_LIVEVIEW = (0x0092F0, 0x00C318, 0x3039F0)


def sites():
    out = []
    for idx, off in enumerate(_VIDEO_A):
        out.append(dict(id='video32_%02d' % idx, group='video_64mbps', offset=off,
                        original=bytes.fromhex('00 36 6e 01 00 2d 31 01'),
                        patched=bytes.fromhex('00 90 d0 03 00 87 93 03')))
    for idx, off in enumerate(_VIDEO_B):
        out.append(dict(id='video16_%02d' % idx, group='video_64mbps', offset=off,
                        original=bytes.fromhex('c0 5d 00 00 20 4e'),
                        patched=bytes.fromhex('00 fa 00 00 60 ea')))
    for idx, off in enumerate(_LIVEVIEW):
        out.append(dict(id='liveview_%02d' % idx, group='liveview_timeout', offset=off,
                        original=bytes.fromhex('a0 bb 0d 00'),
                        patched=bytes.fromhex('80 27 cb 05')))
    return out


def audit(data):
    """Classify reference patches; deliberately never modify `data`."""
    meta = analyze(data)
    result = []
    groups = {}
    for s in sites():
        before, after = s['original'], s['patched']
        off = s['offset']
        observed = data[off:off + len(before)]
        if len(observed) != len(before):
            state = 'out_of_range'
        elif observed == before:
            state = 'original'
        elif observed == after:
            state = 'nikon_hacker_patched'
        else:
            state = 'unexpected'
        item = dict(id=s['id'], group=s['group'],
                    file_offset='0x%X' % off, rom_address='0x%08X' % (BASE + off),
                    state=state, expected_before=before.hex(),
                    expected_after=after.hex(), observed=observed.hex())
        result.append(item)
        groups.setdefault(s['group'], []).append(state)
    summary = {}
    for name, values in groups.items():
        if all(x == 'original' for x in values):
            status = 'original'
        elif all(x == 'nikon_hacker_patched' for x in values):
            status = 'nikon_hacker_patched'
        elif any(x in ('out_of_range', 'unexpected') for x in values):
            status = 'unverified'
        else:
            status = 'mixed'
        summary[name] = dict(status=status, matched_original=values.count('original'),
                             matched_modified=values.count('nikon_hacker_patched'),
                             unexpected=values.count('unexpected'),
                             out_of_range=values.count('out_of_range'))
    return dict(input_sha256=meta['sha256'], base=meta['base'], size=len(data),
                reset_vector=meta['reset_vector'], all_ram_maps_in_bounds=meta['all_maps_in_bounds'],
                source=PATCH_SOURCE, source_patch_count=2, anchor_count=len(result),
                groups=summary, sites=result,
                warning='Anchor verification does not prove camera model, version, safety, or exploitability.')


def thumb_branches(data):
    """Yield 32-bit Thumb immediate branch candidates at halfword boundaries."""
    even_length = len(data) & ~1
    halfwords = memoryview(data)[:max(0, even_length - 2)]
    for index, (first,) in enumerate(struct.iter_unpack('<H', halfwords)):
        if (first & 0xF800) != 0xF000:
            continue
        pos = 2 * index
        second = struct.unpack_from('<H', data, pos + 2)[0]
        opcode = second & 0xD000
        sign = (first >> 10) & 1
        j1 = (second >> 13) & 1
        j2 = (second >> 11) & 1
        pc = BASE + pos + 4
        low = (second & 0x7FF) << 1
        if opcode == 0x8000 and ((first >> 6) & 0xF) < 0xE:
            kind = 't32_b_cond_candidate'
            delta = ((j2 << 19) | (j1 << 18) | ((first & 0x3F) << 12) | low) - (sign << 20)
        elif opcode in (0x9000, 0xD000) or (opcode == 0xC000 and (second & 1) == 0):
            i1 = 1 ^ (j1 ^ sign)
            i2 = 1 ^ (j2 ^ sign)
            delta = ((i1 << 23) | (i2 << 22) | ((first & 0x3FF) << 12) | low) - (sign << 24)
            kind = {0x9000: 't32_b_candidate', 0xD000: 't32_bl_candidate',
                    0xC000: 't32_blx_candidate'}[opcode]
            if opcode == 0xC000:
                pc &= ~3
        else:
            continue
        yield pos, (pc + delta) & 0xFFFFFFFF, kind, (first << 16) | second


def find_refs(data, target, *, limit=100, mode='both', isa='a32'):
    """Find aligned word references and immediate branches as static candidates.

    Potential false positives, because firmware contains embedded data and Thumb
    instructions; these are research candidates, not demonstrated call sites.
    """
    if not (0 <= target <= 0xFFFFFFFF):
        raise ValueError('Target must be a 32-bit address')
    if limit < 1:
        raise ValueError('limit must be positive')
    if isa not in ('a32', 'thumb', 'both'):
        raise ValueError('isa must be a32, thumb or both')
    results = []
    if mode in ('both', 'pointers'):
        for val, tag in ((target, 'arm_word'), (target | 1, 'thumb_word')):
            if tag == 'thumb_word' and val == target:
                continue
            needle = struct.pack('<I', val)
            at = 0
            while len(results) < limit:
                pos = data.find(needle, at)
                if pos == -1:
                    break
                if pos % 4 == 0:
                    results.append(dict(kind='raw_pointer_' + tag,
                                        file_offset='0x%X' % pos,
                                        rom_address='0x%08X' % (BASE + pos)))
                at = pos + 1
    if mode in ('both', 'branches') and isa in ('a32', 'both') and len(results) < limit:
        # A32 branch: bits 27..25 = 101, bits 31..28 != 1111
        for pos in range(0, len(data) - 3, 4):
            inst = struct.unpack_from('<I', data, pos)[0]
            if (inst & 0x0E000000) != 0x0A000000 or (inst >> 28) == 0xF:
                continue
            delta = (inst & 0x00FFFFFF) << 2
            if delta & (1 << 25):
                delta -= 1 << 26
            dest = (BASE + pos + 8 + delta) & 0xFFFFFFFF
            if dest == target:
                results.append(dict(kind='a32_bl_candidate' if inst & 0x01000000 else 'a32_b_candidate',
                                    file_offset='0x%X' % pos,
                                    rom_address='0x%08X' % (BASE + pos),
                                    instruction='0x%08X' % inst))
                if len(results) == limit:
                    break
    if mode in ('both', 'branches') and isa in ('thumb', 'both') and len(results) < limit:
        for pos, dest, kind, inst in thumb_branches(data):
            if dest != (target & ~1) or ((target & 1) and kind == 't32_blx_candidate'):
                continue
            results.append(dict(kind=kind, file_offset='0x%X' % pos,
                                rom_address='0x%08X' % (BASE + pos),
                                instruction='0x%08X' % inst))
            if len(results) == limit:
                break
    return results


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    a = sub.add_parser('anchors', help='Verify 23 documented Nikon Hacker B-firmware patch sites')
    a.add_argument('image', help='Decoded b860_102.bin only')
    a.add_argument('--json', metavar='FILE', help='Save audit report as JSON')
    a.add_argument('--verbose', action='store_true')
    r = sub.add_parser('refs', help='Scan candidate ARM pointers and immediate branch references')
    r.add_argument('image', help='Decoded b860_102.bin only')
    r.add_argument('--target', required=True, type=lambda x: int(x, 0), help='Target ROM address, e.g. 0x90352FE0')
    r.add_argument('--mode', choices=('both', 'pointers', 'branches'), default='both')
    r.add_argument('--isa', choices=('a32', 'thumb', 'both'), default='a32',
                   help='Instruction set for branch scanning (default: a32)')
    r.add_argument('--limit', type=int, default=100)
    r.add_argument('--json', metavar='FILE')
    opts = p.parse_args(argv)
    try:
        data = load(opts.image)
        if opts.command == 'anchors':
            out = audit(data)
            print('D3300 B C1.02 reference-anchor audit — %d checks; SHA256 %s' %
                  (out['anchor_count'], out['input_sha256']))
            for group, info in out['groups'].items():
                print('  %-20s %-22s original=%2d modified=%2d unknown=%2d outside=%2d' %
                      (group, info['status'], info['matched_original'], info['matched_modified'],
                       info['unexpected'], info['out_of_range']))
            for entry in out['sites']:
                if opts.verbose or entry['state'] in ('unexpected', 'out_of_range'):
                    print('  %s %s: %s (got %s)' %
                          (entry['id'], entry['rom_address'], entry['state'], entry['observed']))
        else:
            metadata = analyze(data)
            refs = find_refs(data, opts.target, limit=opts.limit, mode=opts.mode, isa=opts.isa)
            out = dict(target='0x%08X' % opts.target, image_sha256=metadata['sha256'],
                       candidates=refs,
                       warning='Static candidates only: decoding alone does not confirm executable regions.')
            print('Target %s: %d candidate(s) (max %d)' % (out['target'], len(refs), opts.limit))
            for item in refs:
                print('  %-23s %s %s' % (item['kind'], item['rom_address'], item.get('instruction', '')))
        if opts.json:
            Path(opts.json).write_text(json.dumps(out, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
            print('Report:', opts.json)
    except (ValueError, OSError) as e:
        p.exit(2, 'ERROR: %s\n' % e)


if __name__ == '__main__':
    main()
