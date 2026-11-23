# Audit of pasted Gemini findings

Firmware: `b860_102.bin`, SHA-256 `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`.

## Verdict

- **Camera-status/FirmUpdate claim: partly correct.** The object identity remains `17CKRRCCameraStatus` at `0x10491580`; its constructor creates the five-node structure and the object span to the next static object is `0xB0`. The FirmUpdate attribution is not established by adjacency. A separate proven writer at `0x9013BADC` is reached from the storage-controller path `0x900FD56A`, and writes `[base+0xAC]` plus related state fields. This supports storage/status semantics, not a FirmUpdate ownership claim.
- **`CSTBLInfoView` class/factory: class identity and constructor/vtable bytes are plausible static findings, but the claimed end-to-end data provenance to formatters is not proven by the pasted command log alone.** It needs direct call/data-flow evidence from view methods to the property group.
- **Info-property IDs: the pasted mapping is wrong.** The verified table at `0x914ED324` uses `(property_id, size, ram_pointer)` entries. Relevant entries are: `0x104B9044 -> 0x2A`, `0x104B9050 -> 0x5A`, `0x104B9054 -> 0x2D`; the pasted report shifts these IDs and fields. The normalized worker copies the full `0x13C`-byte block; no shutter/aperture/ISO names are assigned from slot order alone.
- **OSD primitive: partly correct.** `0x903111BC` appends records with four halfwords, one byte at offset `+8`, and a 10-byte stride. A second flag byte at `+9` and claims about an EXPEED hardware rasterizer are not proven by this function.
- **T-Kernel stubs: confirmed.** The `ldr pc, [pc, #-4]` / literal pattern begins at `0x90225004`; enumeration gives 471 stubs through `0x90226060`, with RAM kernel targets. This is the strongest fully confirmed item in the pasted report.

No pasted semantic label is promoted to a camera hook or firmware-injection address.
