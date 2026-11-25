# D3300 C 1.02 — literal-pool veneer audit

Firmware: decoded `b860_102.bin`, SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`.

## Proven instruction-level result

Scanning ROM `0x90225000..0x90226070` on four-byte boundaries finds 471 A32
instructions with word `0xE51FF004`, i.e. `ldr pc, [pc, #-4]`. In ARM state,
the literal read is from the word at veneer address `+4`. The first such veneer
is `0x90225004`; the last is `0x90226060`.

Of their literal targets, 467 are in `0x10000000..0x10100000` and four are in
ROM:

| Veneer | Literal target |
| --- | --- |
| `0x90225134` | `0x90025A49` |
| `0x9022513C` | `0x902EB6A1` |
| `0x90225144` | `0x9030D2BD` |
| `0x9022516C` | `0x90274B6D` |

`tools/d3300_tkernel_veneer_audit.py --verify` and
`tests/test_tkernel_veneer_audit.py` reproduce these values against the exact
image hash.

## Boundary

This proves a set of branch veneers and their current literal values. It does
**not** prove that every RAM target is a T-Kernel syscall, identify any service
by name, establish its calling convention, or approve any target for a camera
call/hook. API-name assertions require an independently verified dispatch
table or a direct, instruction-level caller/argument trace.
