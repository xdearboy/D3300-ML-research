# InfoDisp direct-consumer candidate inventory

The read-only scanner `tools/d3300_info_consumer_candidates.py` reduces 1,174
candidate calls to the `CINFOManager` singleton getter to 155 unique immediate
property-query shapes covering 57 of the 79 InfoDisp IDs.

This is a work queue, not a semantic map. A hit means only that a matching
`movs r1, #ID` and nearby indirect call follow a singleton-getter candidate.
Embedded data, overlapping functions and unrelated virtual calls remain
possible. Every candidate still requires local disassembly and a terminal
formatter, standard serializer or concrete lookup table before it may be named.

The highest-volume unresolved IDs are `0x005A` (29 candidates) and `0x005C`
(12). The two already proven IDs remain visible as regression anchors:
`0x0040` has one candidate and `0x0054` has two.
