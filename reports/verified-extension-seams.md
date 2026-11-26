# D3300 C 1.02 — extension-seam classification

Target: decoded `b860_102.bin`, SHA-256
`c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`.

| Candidate | Verified ROM fact | Classification | Static limit |
| --- | --- | --- | --- |
| Active-view state | `0x9002A6A8` writes a view pointer to RAM `0x10123844` | observation candidate | lifetime, synchronization and safe external reads are unproven |
| Menu action router | TBB at `0x90254E1C` selects virtual action slots | control flow only | no physical-button identity or interception contract |
| OSD primitive appender | `0x903111BC` builds 10-byte records | internal call only | ownership, capacity and safe external invocation are unproven |
| Pre-output veneer | `0x90225EA4` branches through RAM target `0x10099244` | dynamic runtime boundary | implementation name, ABI and effects are absent from ROM |

The verified safe-hook count is therefore **zero**. These locations guide
further analysis, but none currently authorizes inserting ARM code, replacing
a vtable entry, consuming live button events, or submitting display records.

`tools/d3300_extension_seams.py --verify` checks every row against the exact
firmware image.
