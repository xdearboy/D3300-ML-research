# D3300 B-firmware RAM mappings from Nikon Hacker FirmwareNotes.md
# @category Nikon.D3300
#
# Run this Ghidra script ONLY after importing the unpacked B-firmware as RAW
# ARMv7 LE, with file offset 0 mapped to 0x90020000. Save a Ghidra backup.
# The script changes only the local Ghidra database, never the firmware file.

BASE = 0x90020000
MAPS = [
    ('D3300_KERNEL_CODE', 0x9158A358, 0x10000000, 0x0003B5B4, True),
    ('D3300_KERNEL_RW1', 0x915C590C, 0x1003B5B4, 0x0000179C, False),
    ('D3300_KERNEL_RW2', 0x916A73A0, 0x1011D420, 0x00028B3C, False),
    ('D3300_KERNEL_RW3', 0x916A739C, 0x1011D03C, 4, False),
]

mem = currentProgram.getMemory()
addr = currentProgram.getAddressFactory().getDefaultAddressSpace()

def a(n):
    return addr.getAddress(n)

rom = mem.getBlock(a(BASE))
if rom is None or rom.getStart() != a(BASE):
    raise RuntimeError('Expected unpacked B-firmware RAW mapped at 0x90020000')

for name, src, dst, size, executable in MAPS:
    source_start = a(src)
    source_end = a(src + size - 1)
    if mem.getBlock(source_start) is None or mem.getBlock(source_end) is None:
        raise RuntimeError('Missing ROM source for ' + name + ' at ' + str(source_start))
    existing = mem.getBlock(a(dst))
    if existing is not None:
        print('SKIP: destination already occupied: {} {}'.format(name, existing.getName()))
        continue
    block = mem.createByteMappedBlock(name, a(dst), a(src), size, False)
    block.setRead(True)
    block.setWrite(True)  # actual destination RAM is writable (including the executable kernel copy)
    block.setExecute(executable)
    block.setComment('Nikon Hacker D3300 C1.02 ROM-to-RAM source map. Static analysis only.')
    print('Mapped {}: {} -> {} size=0x{:X}'.format(name, hex(src), hex(dst), size))
print('Done. Review memory map, then launch Auto Analyze.')
