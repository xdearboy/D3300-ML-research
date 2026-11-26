# InfoDisp properties 0x005A and 0x005C

Static evidence classifies both properties as UI state gates, not photographic
values.

- `0x005A`: 29 direct-query candidates. At `0x90252240` the value is bounded
  below 14 and drives a TBB. Other consumers compare it with members of the
  same small state domain, including `0`, `1`, `2`, `3`, `5`, `7`, `10`,
  `11`, `12` and `13`.
- `0x005C`: 12 direct-query candidates. Several paths, including
  `0x9025428C`, read it only after `0x005A == 13`, then use it in further
  equality tests or a bounded TBB.
- `0x90258654` and `0x90258668` are adjacent four-byte getter wrappers for the
  two IDs.

No inspected chain reaches a concrete format string, standardized PTP
serializer, APEX conversion table or named enum. Therefore individual state
names and photographic meanings remain **UNPROVEN**. The machine check is
`tools/d3300_info_state_gates.py --verify`.
