#!/usr/bin/env python3
"""Read-only Nikon DSLR firmware container decoder.

Reimplementation of the legacy three-level XOR format in Nikon Hacker's
firmware decode/Firmware.cs by Simeon Pilgrim. Firmware files are user-provided.
DOES NOT produce a camera-installable update.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

# Nikon Hacker Firmware.cs Xor_Ord1, Xor_Ord2, Xor_Ord3.
# Each table is 256 entries; entries are byte values except one 0x100 in ord3
# (the cast to byte in the reference implementation makes that entry 0).
ORD1 = bytes.fromhex('''
E8 FE 46 E3 7D AB 5C B9 A5 53 C4 C7 32 CD 9C ED
94 67 4E 5B 48 3A 52 B6 34 F7 8E 12 90 98 82 3C
09 2B 68 A8 24 D5 10 9D 9A D9 A9 7F 50 4B DD 9E
62 1C 6A 64 02 11 DA 4A 6E 35 BD A0 B1 D7 DE 83
5D E5 5E CC DB AC 18 E2 25 69 07 BC 39 97 14 EB
B2 73 36 8A 99 B8 1E 2E EF 93 BA EE F5 C5 7B 74
8D E1 C3 4F 41 42 6C E6 A2 06 6F 85 2A 2F 1B 38
08 AF 44 00 33 63 91 22 87 70 B0 43 B5 66 E0 FF
30 AD 8F C1 F4 EA F9 F6 51 D6 D4 3E 04 72 3D 54
78 C0 7E 26 FA 56 58 C9 55 C8 A6 16 23 84 B7 CB
45 7C D8 7A 27 2D CA 03 3F 17 0B 57 BB 3B F0 49
1F D1 86 80 95 20 6B C6 BF AA 79 D2 75 CF AE D0
5F F1 61 F3 FB CE 29 65 0F 31 2C FD 76 0C 4C 4D
60 F8 88 6D A4 EC 9B 92 47 A1 E4 21 FC 81 A7 C2
0E A3 40 0D 8B 59 96 37 E7 F2 77 1D 28 0A 8C 5A
15 9F 01 B3 D3 BE B4 1A 89 E9 13 05 DF DC 71 19
''')
ORD2 = bytes.fromhex('''
76 0F 43 D9 DB DC 9B 49 4E 42 B7 9F EC 55 19 11
58 23 69 A2 B8 68 E8 2B 91 F3 1A 34 ED 0A 06 89
B2 79 2A C8 EE A3 B5 D0 FD 17 F9 CE 74 39 47 C5
C1 5D 86 7F 6A AB E5 F5 C9 96 71 1C 09 25 D3 8C
0C 02 B1 48 7C 46 3E 08 7B 01 54 6B B9 4F CD F1
51 50 59 A4 A7 6C 3F B6 9D BC 4C 9E 16 37 A1 C0
6E E2 DA 3D 22 CB E7 5B 98 53 92 36 90 AC 31 24
21 C6 63 35 B4 5C 1F 77 4A E4 0D 13 8D C7 99 7E
81 3C 60 28 F0 BF 82 2C 78 7A 5F 93 84 70 EA 9A
8E D2 27 E0 CF 6D 10 9C 56 07 12 FA 26 97 80 E3
E1 61 8A 75 A9 5A DE 1E 5E 4D 66 0E BA 4B 20 40
A8 8F 52 7D DF E6 AF 6F BE FC 94 A0 3A 33 45 14
62 00 87 AE B3 8B D4 CA FF E9 04 88 CC 41 D7 D6
BB 95 32 18 F8 72 65 3B 29 AD 44 1B C2 D8 1D A5
DD 67 D5 30 A6 EF 2F F2 83 2D 03 BD 15 2E D1 73
57 AA FB 85 F4 64 0B C4 F7 EB 38 C3 F6 05 FE B0
''')
ORD3 = bytes.fromhex('''
2F 25 06 BB E3 82 70 CE 86 FB 00 3A F8 BF 76 F5
C3 F6 9D 9E 10 EA FC 9C DD 12 46 93 4F A6 AD 68
94 B8 22 E1 62 5B CF 67 08 E7 48 0C 7E 7D B1 A9
3E 1B C5 0A 11 AA BA B2 D0 79 A5 DB C6 6A A3 D4
83 55 87 9B 5C 27 AB 77 36 42 61 2D B4 4D 1A 9F
64 8F D5 43 05 24 72 02 41 15 6D 6E 47 26 65 13
49 95 CB 71 EF 6F 2C BD FD AC 31 DE 2A 5E 29 A2
BC 28 C7 56 C8 16 75 E4 19 F2 BE 37 90 E8 3B 92
35 4B D2 D7 D8 0E 51 8B B0 EC 4C B9 59 2E 66 54
B5 D1 E5 1E 0F 5A C4 CD EB 4E 7C 74 8E FA 81 E2
7F 9A A8 5F 32 89 98 07 39 C0 38 1F 01 E0 8D 1D
F3 96 57 A0 0B ED 09 18 8C F4 0D 21 D9 23 78 A1
C1 FE 3C 30 73 5D 40 99 B6 6C 7B 14 CA C2 E6 8A
E9 AE 7A D6 F1 3D A4 34 2B DA 63 84 B3 AF 88 B7
45 97 DC 20 17 3F 4A 52 03 33 DF D3 04 69 91 F0
F9 CC 50 44 FF 80 58 F7 C9 60 6B 53 A7 85 EE 1C
''')
assert len(ORD1) == len(ORD2) == len(ORD3) == 256


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def unxor(container: bytes, offset: int = 0) -> bytes:
    """Nikon Hacker DecodePackageFile: indices start at 0 AFTER offset."""
    if offset < 0 or offset >= len(container):
        raise ValueError('invalid offset')
    src = memoryview(container)[offset:]
    result = bytearray(len(src))
    # 256*256-byte blocks. Use translate to keep Python runtime acceptable.
    # Higher-order ord3 changes each 65536 bytes.
    for base in range(0, len(src), 65536):
        size = min(65536, len(src) - base)
        block = src[base:base+size]
        o3 = ORD3[(base >> 16) & 255]
        for sub in range(0, size, 256):
            chunk = block[sub:sub+256]
            o2 = ORD2[((base + sub) >> 8) & 255]
            mask = o2 ^ o3
            result[base+sub:base+sub+len(chunk)] = bytes(x ^ ORD1[i] ^ mask for i, x in enumerate(chunk))
    return bytes(result)


def find_components(data: bytes):
    # Firmware.cs reads header at 0x20, entries 32 bytes, u32 endian Nikon-specific.
    # Validate both candidates against actual file bounds and safe filenames.
    for endian in ('>', '<'):
        count, headerlen, d1, d2 = struct.unpack_from(endian+'IIII', data, 0x20)
        if not (1 <= count <= 16 and 0x20 <= headerlen <= 4096):
            continue
        entries = []
        okay = True
        for j in range(count):
            off = 0x30 + 32*j
            if off+32 > len(data):
                okay=False;break
            name_b = data[off:off+16].split(b'\0',1)[0]
            try:
                name=name_b.decode('ascii')
            except UnicodeDecodeError:
                okay=False;break
            if not re.fullmatch(r'[a-zA-Z0-9_.-]{1,16}',name):
                okay=False;break
            start, length, h1, h2 = struct.unpack_from(endian+'IIII',data,off+16)
            if not (length > 1024 and start >= 0x20 and start+length <= len(data)):
                okay=False;break
            entries.append({'name':name,'offset':start,'length':length,'unknown1':h1,'unknown2':h2})
        if okay:
            return {'endian':'big' if endian=='>' else 'little','count':count,'headerlen':headerlen,'unknown1':d1,'unknown2':d2,'entries':entries}
    raise ValueError('decoded header invalid (unexpected format or wrong decode offset/key)')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('firmware',type=Path)
    p.add_argument('-o','--out',type=Path,required=True)
    p.add_argument('--offset',type=lambda s:int(s,0),default=0,help='decode offset, normally 0 for D3300')
    p.add_argument('--keep-decoded-container',action='store_true')
    a=p.parse_args()
    raw=a.firmware.read_bytes()
    print('Source:',a.firmware,'bytes=',len(raw),'SHA256=',sha256(raw))
    decoded=unxor(raw,a.offset)
    print('Decoded first 96 bytes:',decoded[:96].hex(' '))
    info=find_components(decoded)
    a.out.mkdir(parents=True,exist_ok=True)
    info.update({'input_filename':a.firmware.name,'input_bytes':len(raw),'input_sha256':sha256(raw),'decoded_sha256':sha256(decoded),'decode_offset':a.offset})
    for e in info['entries']:
        blob=decoded[e['offset']:e['offset']+e['length']]
        (a.out/e['name']).write_bytes(blob)
        e['sha256']=sha256(blob)
        print(e['name'],'len',len(blob),'offset',hex(e['offset']),'sha256',e['sha256'])
    (a.out/'manifest.json').write_text(json.dumps(info,indent=2)+'\n')
    if a.keep_decoded_container:
        (a.out/'decoded_container.bin').write_bytes(decoded)
    print('Manifest:',a.out/'manifest.json')

if __name__=='__main__':
    main()
