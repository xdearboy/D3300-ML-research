# InfoDisp 0x005A state-route reduction

The TBB at `0x90252248` accepts fourteen values (`0..13`) but contains only
two unique route targets:

| `0x005A` values | Target | Next queried property |
| --- | --- | --- |
| `0..3` | `0x9025225A` | `0x031E` |
| `4..13` | `0x9025226A` | `0x031F` |

Both routes merge at `0x90252278`, perform the same indirect property getter,
and then apply shared boolean gating. This reduces fourteen manual traces to
two unique branches.

The routing and numeric IDs are proven; names for the states and properties
`0x031E/0x031F` are not. `tools/d3300_state_route_analyzer.py --verify`
reconstructs the table directly from the exact firmware image.
