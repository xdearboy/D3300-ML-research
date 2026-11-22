#!/usr/bin/env python3
"""Export decoded D3300 B firmware to a STATIC-ANALYSIS ELF32 ARM file.

No Nikon code is included with this tool. Output is deliberately a diagnostic
ELF, NEVER a firmware-update image. Nothing here writes to a camera.
"""
import argparse
import struct
from pathlib import Path
from d3300_lab import BASE, analyze, load

TEXT_OFFSET = 0x100
SHSTRTAB = b'\x00.text\x00.shstrtab\x00'


def align4(n):
    return (n + 3) & ~3


def export_elf(data):
    info = analyze(data)
    if not info['all_maps_in_bounds']:
        raise ValueError('Decoded B image too small for known D3300 C 1.02 ROM-to-RAM ranges')
    reset = int(info['reset_vector'], 16)
    names_at = TEXT_OFFSET + len(data)
    shoff = align4(names_at + len(SHSTRTAB))
    ident = b'\x7fELF' + bytes([1, 1, 1, 0]) + bytes(8)  # ELF32 LE System V
    # ET_EXEC, ARM EM=40, ELF version 1, Nikon B reset entry, EABI 5
    header = struct.pack('<16sHHIIIIIHHHHHH',
                         ident, 2, 40, 1, reset, 52, shoff, 0x05000000,
                         52, 32, 1, 40, 3, 2)
    # PT_LOAD: single RX image located at Nikon B-firmware ROM address.
    ph = struct.pack('<IIIIIIII', 1, TEXT_OFFSET, BASE, BASE,
                     len(data), len(data), 5, 0x100)
    empty = bytes(40)
    text_sh = struct.pack('<IIIIIIIIII', 1, 1, 0x6, BASE,
                          TEXT_OFFSET, len(data), 0, 0, 4, 0)
    names_sh = struct.pack('<IIIIIIIIII', 7, 3, 0, 0,
                           names_at, len(SHSTRTAB), 0, 0, 1, 0)
    result = bytearray(shoff + 120)
    result[:len(header)] = header
    result[52:52 + 32] = ph
    result[TEXT_OFFSET:TEXT_OFFSET + len(data)] = data
    result[names_at:names_at + len(SHSTRTAB)] = SHSTRTAB
    result[shoff:shoff + 120] = empty + text_sh + names_sh
    return bytes(result)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', help='Decoded b860_102.bin (NOT D3300_0102.bin update)')
    p.add_argument('-o', '--output', required=True, help='Analysis-only .elf destination')
    a = p.parse_args(argv)
    try:
        source = Path(a.image).resolve()
        target = Path(a.output).resolve()
        if source == target:
            raise ValueError('Input and output must be different files')
        if target.suffix.lower() != '.elf':
            raise ValueError('Output MUST end with .elf (never .bin)')
        if target.exists():
            raise ValueError('Output already exists; refusing to overwrite')
        blob = export_elf(load(source))
        target.write_bytes(blob)
        print('Wrote static-analysis ELF:', target)
        print('Size:', len(blob), 'bytes; text @ 0x%08X' % BASE)
        print('NOT a Nikon firmware update: DO NOT FLASH IT.')
    except (ValueError, OSError) as e:
        p.exit(2, 'ERROR: %s\n' % e)


if __name__ == '__main__':
    main()
