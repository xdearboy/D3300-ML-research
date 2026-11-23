# D3300-ML research handoff

## Current status

The repository combines reproducible static-analysis work with host-only prototypes. Earlier documentation incorrectly presented several prototypes as verified camera integrations. This file records the corrected boundary.

No installable firmware, ARM payload image, linker script, relocation loader, or verified camera execution path exists in this repository.

## Findings worth retaining

- `73f8e71`: identifies RAM `0x10491580` as `CKRRCCameraStatus`, not a button producer. The vtable, RTTI and read accessor path are supported by disassembly.
- `8677c06` and `70e6091`: add and harden derived-reference analysis with unit tests.
- `reports/D3300_C102_info_disp_data.md`: verifies the separate `CKRRCInfoDispData` instance at `0x10492714` and its eleven 52-byte snapshots. This is the current source candidate for stock shooting-information values.
- `reports/verified-info-disp-property-map.json`: verifies the `CKRRCInfoDispDataGrp` table and maps all 79 normalized output slots at `0x104B9044..0x104B917C` to numeric property IDs. Meanings such as ISO/shutter/aperture remain deliberately unassigned until a consumer path proves them.
- The reports contain useful static traces for RelayEventEx, COPNO objects, the 16-byte AppButton state handoff, menu dispatch and DrawBuffer routines.
- T-Kernel identification is plausible from firmware strings/configuration. Exact syscall names, veneers and task-management integration must be independently rechecked before being promoted to verified.

## Unverified or disproven claims

- The shooting-field layout modeled in `DmlHwCameraStatusRaw` is a simulator fixture. `reports/D3300_C102_camera_status_ring.md` proves that `+0x84` selects a ring node pointing to an 8-byte bitfield snapshot and that `+0x8C..+0x98` hold transition indicators, not direct exposure values.
- ARGB8888, 720x480 as the active OSD surface, and field meanings of the 20-byte DrawBuffer entries are not established by the cited disassembly.
- Numeric MENU/INFO/directional button mappings remain candidates unless a report explicitly traces the physical producer.
- `payload/` is a host lifecycle model, not relocatable camera code.
- The PTP scanner reports raw 16-bit byte-pattern occurrences. It does not prove that D3300 implements Z6II service-wrap memory operations.
- Generated `.NOP/.NCP` files are not proven Nikon-compatible. Tests validate only the repository's own format assumptions.
- Focus peaking, zebra and histogram operate on synthetic host buffers; no live-view acquisition or camera display hook is implemented.
- Firmware-update and SD diagnostic reports contain hypotheses that require fresh disassembly review before use.

## Verification

Run:

```bash
make clean && make test
git diff --check
```

Passing tests validate host behavior only. A camera-facing statement needs a recorded firmware hash, instruction bytes/disassembly, control/data-flow explanation and an explicit confidence level.

## Recommended next work

1. Re-disassemble each claimed T-Kernel veneer and record its calling convention without borrowing names from another Nikon model.
2. Trace producers and consumers of the DrawBuffer descriptor fields before assigning C types or a pixel format.
3. Trace the pointer returned by `CKRRCCameraStatus` slot `+0x14`; derive fields from actual loads/stores rather than from UI expectations.
4. Replace the PTP byte-pattern scan with dispatch-table and control-flow validation, or label every output as an unverified occurrence.
5. Keep camera execution, SD import and firmware creation out of scope.
