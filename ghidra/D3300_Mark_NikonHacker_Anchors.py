# D3300 Nikon Hacker C 1.02: 23 existing patch anchors, for static research.
# @category Nikon.D3300
#
# Run on decoded B firmware loaded at 0x90020000 (Raw Binary or analysis ELF).
# Creates bookmarks only, never modifies firmware or writes to camera.
# This is NOT a hook locator; many anchors are literal constants/tables.

BASE = 0x90020000
VIDEO32 = [0x143C8BC, 0x143C8CC, 0x143C8D4, 0x143C8E4,
           0x143C8F4, 0x143C904, 0x143C914]
VIDEO16 = [0x143C2C4, 0x143C2DC, 0x143C2F4, 0x143C30C,
           0x143C324, 0x143C348, 0x143C360, 0x143C36C,
           0x143C384, 0x143C39C, 0x143C3B4, 0x143C3CC,
           0x143C414]
LIVEVIEW = [0x0092F0, 0x00C318, 0x3039F0]

addrspace = currentProgram.getAddressFactory().getDefaultAddressSpace()
mem = currentProgram.getMemory()
bm = currentProgram.getBookmarkManager()

def at(n):
    return addrspace.getAddress(n)

def actual(o, length):
    base = at(BASE + o)
    return [mem.getByte(base.add(i)) & 0xff for i in range(length)]

def byte_list(xs):
    return [int(x, 16) for x in xs.split()]

if mem.getBlock(at(BASE)) is None:
    raise RuntimeError('B-firmware not mapped at 0x90020000; aborting')
if actual(0, 4) != byte_list('18 f0 9f e5'):
    raise RuntimeError('Expected ARM B-firmware reset header not found; aborting')

sets = [
    ('VIDEO_64M_TABLE_8', VIDEO32, '00 36 6e 01 00 2d 31 01', '00 90 d0 03 00 87 93 03'),
    ('VIDEO_64M_TABLE_6', VIDEO16, 'c0 5d 00 00 20 4e', '00 fa 00 00 60 ea'),
    ('LIVEVIEW_TIMEOUT', LIVEVIEW, 'a0 bb 0d 00', '80 27 cb 05'),
]

for name, offsets, before, after in sets:
    original, changed = byte_list(before), byte_list(after)
    for i, o in enumerate(offsets):
        where = at(BASE + o)
        try:
            got = actual(o, len(original))
        except Exception as err:
            print('SKIP {} +0x{:X}: {}'.format(name, o, err))
            continue
        status = 'ORIGINAL' if got == original else ('NIKON_HACKER_MODIFIED' if got == changed else 'UNVERIFIED')
        comment = '{}_{} — {}; decoded B offset +0x{:X}; Nikon Hacker reference constant, not a confirmed hook'.format(
            name, i, status, o)
        bm.setBookmark(where, 'Note', 'D3300 NikonHacker', comment)
        print('0x{:08X}: {}'.format(BASE + o, comment))

reset = at(0x90352FE0)
if mem.getBlock(reset) is not None:
    bm.setBookmark(reset, 'Note', 'D3300 Boot', 'Reset entry described in Simeon Pilgrim firmware analysis (verify code in Ghidra)')
print('Done: bookmarks are research annotations only. Do not flash anything.')
