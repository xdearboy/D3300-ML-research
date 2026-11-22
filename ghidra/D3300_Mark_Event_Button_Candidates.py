# D3300 C 1.02 event/button candidates verified by static disassembly.
# @category Nikon.D3300
#
# Bookmarks only. Names describe observed instruction behavior and do not claim
# a stable Nikon API or a safe firmware hook.

BASE = 0x90020000
CANDIDATES = [
    (0x900223A4, '04 f0 47 fe', 'RelayEvent init call',
     'BL 0x90027036; followed by BL 0x900271E4 for the Ex task'),
    (0x9002702C, '80 47', 'RelayEvent callback dispatch',
     'BLX R0 after mask test; callback slot was read from RAM'),
    (0x900271DA, '80 47', 'RelayEventEx callback dispatch',
     'BLX R0 after mask test; callback slot was read from RAM'),
    (0x9013CF56, '30 b5 04 46', 'generic button normalization',
     'COPNOButton vtable method; switches over event IDs'),
    (0x9013DB1E, '4a 68 09 88', 'shutter button handler',
     'COPNOShutterButton vtable method; forwards ID and payload'),
    (0x9013DDCC, '2d e9 f0 41', 'queued-button handler',
     'COPNOQueueButton vtable method; handles event IDs'),
    (0x90327520, '10 b5 00 23', 'AppButtonController constructor',
     'CSYSCAppButtonController vtable-associated constructor'),
    (0x9032760C, '2d e9 ff 47', 'AppButtonController queue loop',
     'receives button-controller records and updates state'),
    (0x903276B4, '29 f0 f4 fd', 'AppButtonController event handoff',
     'BL 0x903512A0 with the received record at SP'),
]

space = currentProgram.getAddressFactory().getDefaultAddressSpace()
memory = currentProgram.getMemory()
bookmarks = currentProgram.getBookmarkManager()

def address(value):
    return space.getAddress(value)

def observed(value, length):
    start = address(value)
    return [memory.getByte(start.add(i)) & 0xff for i in range(length)]

if memory.getBlock(address(BASE)) is None:
    raise RuntimeError('B-firmware not mapped at 0x90020000; aborting')

for value, expected_text, name, evidence in CANDIDATES:
    expected = [int(item, 16) for item in expected_text.split()]
    got = observed(value, len(expected))
    if got != expected:
        raise RuntimeError(
            'Byte mismatch at 0x{:08X}: expected {}, got {}'.format(
                value, expected_text, ' '.join('{:02x}'.format(item) for item in got)))
    comment = '{} — {}. Static candidate only; ABI and hook safety unverified.'.format(
        name, evidence)
    bookmarks.setBookmark(address(value), 'Note', 'D3300 Event/Button', comment)
    print('0x{:08X}: {}'.format(value, comment))

print('Done: {} event/button candidates marked; firmware was not modified.'.format(
    len(CANDIDATES)))
