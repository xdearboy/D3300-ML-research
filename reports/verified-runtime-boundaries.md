# D3300 C 1.02 — static/runtime boundary ledger

Target: decoded `b860_102.bin`, SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`.

| Boundary | ROM evidence | Runtime state | Static conclusion |
| --- | --- | --- | --- |
| Input channel callback | literal `0x9011E188` = `0x10319D2C` | callback field at `+0x18` | physical event translation cannot be recovered from the ROM value of this RAM table |
| Info property subscribers | literal `0x90169B24` = `0x10138B84`; code subtracts `0x70` | 8-byte registry entries from `0x10138B14` | subscriber identities and callback targets require post-boot RAM contents |
| GUI pre-output veneer | `0x90225EA4` = `ldr pc, [pc, #-4]`; literal `0x90225EA8` = `0x10099244` | implementation at RAM target | service name, ABI and side effects are not established by the veneer |
| `CGUICDraw` object | literal `0x9012ADB4` = `0x10383580`; constructor `0x9012AD16` | selected output-object pointers | static construction is proven; concrete output implementations remain runtime-selected |

`tools/d3300_runtime_boundaries.py --verify` checks these anchors against the
exact image. These boundaries explain why ROM-only analysis cannot assign a
physical MENU/OK identity, enumerate live InfoDisp subscribers, or establish a
camera display ABI.
