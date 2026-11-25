# Nikon D3300 C 1.02 — Verified UI Routing Catalogue & EventParam Lifecycle

Static reverse engineering analysis of the GUI event routing architecture, TBB branch tables, vtable dispatch slots, and `CJAPFButtonEventParam` lifecycle in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. Architectural Overview & Dispatch Topology

The menu event processing system on the D3300 follows a 3-stage table-branch (TBB) routing pipeline:

```
[Raw Event Record] (16 bytes, <HHIII from COPNOHandler queue)
       │
       ▼
1. TBB Table 1 (0x90252DAA, 59 entries)
   Maps raw input codes (0..58) to 40 logical IDs (1..28 and 33..44)
       │
       ▼
2. Parameter Factory
   Factory `0x9012B82A` returns either an object pointer or zero, and on its
   success path calls initializer `0x9012B810`. The allocator/pool ABI behind
   its first helper (`0x900F01FC`) remains unproven. `param+0x0C` receives
   `record.value_1` on the traced controller path.
       │
       ▼
3. Screen Lookup & Dispatch (0x90032AF8)
   Fetches active view from CGUICEntry container (0x9003FF50)
   Calls active view slot +0x10 (0x9025A85E)
       │
       ▼
4. TBB Table 2 (0x9025A86E, 100 entries)
   Routes all 40 logical IDs (1..28, 33..44) to common target 0x9025A8D2
   Calls slot +0xC4 pre-event hook, then tail-calls slot +0x40 (OnButtonEvent)
       │
       ▼
5. Screen Event Router (slot +0x40 at 0x9025AD54)
   Calls slot +0x54 (pre-action hook)
   Calls slot +0x44 (DispatchButtonAction at 0x90254DD4)
       │
       ▼
6. TBB Table 3 (0x90254E1C, 29 entries)
   Routes logical IDs (1..28) to dedicated vtable slots +0x120..+0x184
       │
       ▼
7. Post-dispatch virtual call (0x90032C80..0x90032C86)
   Calls vtable slot +0x00 on the object held in r4; object identity and lifetime effect require a full register trace
```

---

## 2. Table-Branch Table 1: Raw Input to Logical ID Translation

Located at `0x90252DAA` (branch instruction `tbb [pc, r1]` at `0x90252DA6`).
Bounds check: `0x90252D9E: cmp r1, #0x3B` (59 entries, codes 0..58).
Out-of-bounds or unmapped entries branch to `0x90252E56` (`bx lr`, returning 0).

### Complete Mapping Table (59 Entries)

| Raw Code | Byte Offset | Target PC | Logical ID | Status | Category |
|:---:|:---:|:---:|:---:|:---:|:---|
| `0x00` (0) | `+0xA2` (81) | `0x90252E4C` | `23` (`0x17`) | Mapped | Standard Control |
| `0x01` (1) | `+0x6E` (55) | `0x90252E18` | `13` (`0x0D`) | Mapped | Standard Control |
| `0x02` (2) | `+0x72` (57) | `0x90252E1C` | `15` (`0x0F`) | Mapped | Standard Control |
| `0x03` (3) | `+0x76` (59) | `0x90252E20` | `16` (`0x10`) | Mapped | Standard Control |
| `0x04` (4) | `+0x7A` (61) | `0x90252E24` | `17` (`0x11`) | Mapped | Standard Control |
| `0x05` (5) | `+0x7E` (63) | `0x90252E28` | `18` (`0x12`) | Mapped | Standard Control |
| `0x06` (6) | `+0x82` (65) | `0x90252E2C` | `19` (`0x13`) | Mapped | Standard Control |
| `0x07` (7) | `+0x86` (67) | `0x90252E30` | `20` (`0x14`) | Mapped | Standard Control |
| `0x08` (8) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x09` (9) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x0A` (10) | `+0x8A` (69) | `0x90252E34` | `21` (`0x15`) | Mapped | Standard Control |
| `0x0B` (11) | `+0x96` (75) | `0x90252E40` | `27` (`0x1B`) | Mapped | Menu Toggle / Back |
| `0x0C` (12) | `+0x3C` (30) | `0x90252DE6` | `1` (`0x01`) | Mapped | Confirm / Action |
| `0x0D` (13) | `+0x40` (32) | `0x90252DEA` | `2` (`0x02`) | Mapped | Action 02 |
| `0x0E` (14) | `+0x46` (35) | `0x90252DF0` | `3` (`0x03`) | Mapped | Action 03 |
| `0x0F` (15) | `+0x4A` (37) | `0x90252DF4` | `4` (`0x04`) | Mapped | Action 04 |
| `0x10` (16) | `+0x4E` (39) | `0x90252DF8` | `5` (`0x05`) | Mapped | Nav Right |
| `0x11` (17) | `+0x52` (41) | `0x90252DFC` | `6` (`0x06`) | Mapped | Nav Left |
| `0x12` (18) | `+0x56` (43) | `0x90252E00` | `7` (`0x07`) | Mapped | Nav Up |
| `0x13` (19) | `+0x5A` (45) | `0x90252E04` | `8` (`0x08`) | Mapped | Nav Down |
| `0x14` (20) | `+0x5E` (47) | `0x90252E08` | `9` (`0x09`) | Mapped | Action 09 |
| `0x15` (21) | `+0x62` (49) | `0x90252E0C` | `10` (`0x0A`) | Mapped | Action 10 |
| `0x16` (22) | `+0x66` (51) | `0x90252E10` | `11` (`0x0B`) | Mapped | Action 11 |
| `0x17` (23) | `+0x6A` (53) | `0x90252E14` | `12` (`0x0C`) | Mapped | Action 12 |
| `0x18` (24) | `+0x92` (73) | `0x90252E3C` | `26` (`0x1A`) | Mapped | Action 26 |
| `0x19` (25) | `+0x8E` (71) | `0x90252E38` | `25` (`0x19`) | Mapped | Action 25 |
| `0x1A` (26) | `+0x9A` (77) | `0x90252E44` | `28` (`0x1C`) | Mapped | Action 28 |
| `0x1B` (27) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x1C` (28) | `+0xB2` (89) | `0x90252E5C` | `33` (`0x21`) | Mapped | Extended Input 33 |
| `0x1D` (29) | `+0xB6` (91) | `0x90252E60` | `34` (`0x22`) | Mapped | Extended Input 34 |
| `0x1E` (30) | `+0xBA` (93) | `0x90252E64` | `35` (`0x23`) | Mapped | Extended Input 35 |
| `0x1F` (31) | `+0xBE` (95) | `0x90252E68` | `36` (`0x24`) | Mapped | Extended Input 36 |
| `0x20` (32) | `+0xC2` (97) | `0x90252E6C` | `37` (`0x25`) | Mapped | Extended Input 37 |
| `0x21` (33) | `+0xC6` (99) | `0x90252E70` | `38` (`0x26`) | Mapped | Extended Input 38 |
| `0x22` (34) | `+0xCA` (101) | `0x90252E74` | `39` (`0x27`) | Mapped | Extended Input 39 |
| `0x23` (35) | `+0xCE` (103) | `0x90252E78` | `40` (`0x28`) | Mapped | Extended Input 40 |
| `0x24` (36) | `+0xD2` (105) | `0x90252E7C` | `41` (`0x29`) | Mapped | Extended Input 41 |
| `0x25` (37) | `+0xD6` (107) | `0x90252E80` | `42` (`0x2A`) | Mapped | Extended Input 42 |
| `0x26` (38) | `+0xDA` (109) | `0x90252E84` | `43` (`0x2B`) | Mapped | Extended Input 43 |
| `0x27` (39) | `+0xDE` (111) | `0x90252E88` | `44` (`0x2C`) | Mapped | Extended Input 44 |
| `0x28` (40) | `+0x9E` (79) | `0x90252E48` | `14` (`0x0E`) | Mapped | Action 14 |
| `0x29` (41) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x2A` (42) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x2B` (43) | `+0xA6` (83) | `0x90252E50` | `22` (`0x16`) | Mapped | Action 22 |
| `0x2C..0x39` (44..57) | `+0xAC` (86) | `0x90252E56` | — | Unmapped | Default NOP Return |
| `0x3A` (58) | `+0xAE` (87) | `0x90252E58` | `24` (`0x18`) | Mapped | Action 24 |

---

## 3. Table-Branch Table 2: Active View Screen Event Filter

Located at `0x9025A86E` (branch instruction `tbb [pc, r1]` at `0x9025A86A`).
Bounds check: `0x9025A860: cmp r1, #0x64` (100 entries, logical IDs 0..99).
Out-of-bounds entries branch to `0x9025A952` (`pop {r4..r6, pc}`).

### Routing Target Breakdown

1. **Target `0x9025A8D2` (Primary Button Handler)**:
   - Handled IDs: Exactly 40 logical IDs: `1..28` and `33..44`.
   - Execution:
     ```arm
     0x9025A8D2: cmp   r5, #0              ; check CJAPFButtonEventParam != NULL
     0x9025A8D4: beq   0x9025a952          ; if null, bail out
     0x9025A8D6: ldr   r0, [r4]            ; active view vtable
     0x9025A8D8: movs  r1, #0
     0x9025A8DA: ldr.w r2, [r0, #0xc4]     ; slot +0xC4 (PreEventHook)
     0x9025A8DE: mov   r0, r4              ; this
     0x9025A8E0: blx   r2                  ; call PreEventHook
     0x9025A8E2: ldr   r0, [r4]            ; vtable
     0x9025A8E4: ldr   r3, [r0, #0x40]     ; slot +0x40 (OnButtonEvent)
     0x9025A8E6: b     0x9025a91e          ; dispatch blx r3 with (this, id, param)
     ```

2. **Unhandled NOP Target `0x9025AA2E`**:
   - Handled IDs: `0, 31, 32, 47, 48, 49, 50, 51, 64, 87, 89, 90, 95`.
   - Execution: restores registers and returns 0.

3. **Special View Event Targets**:
   - Handled IDs: `29, 30, 45, 46, 52, 53..63, 65..86, 88, 91..94, 96..99` route to non-button UI view update routines.

---

## 4. Table-Branch Table 3: View Action Router & Controller VTable Slots

Located at `0x90254E1C` (branch instruction `tbb [pc, r5]` at `0x90254E18`).
Called by `CMENUTopMenuView::DispatchButtonAction` (slot `+0x44` at `0x90254DD4`).
Bounds check: `0x90254E14: cmp r5, #0x1D` (29 entries, logical IDs 0..28).
Out-of-bounds entries branch to `0x90254E70` (returns 1).

### Slot Dispatch Table (29 Entries)

| Logical ID | Target PC | VTable Slot | CMENUTopMenuView Handler (`0x9151D874`) | Inferred Semantic Role | Confidence |
|:---:|:---:|:---:|:---:|:---|:---:|
| `0` (`0x00`) | `0x90254E70` | — | None (Returns 1) | Ignored / Bypass | **PROVEN** |
| `1` (`0x01`) | `0x90254E7E` | `+0x12C` | `0x90076FF4` | Confirm / Select / Action | **INFERENCE** |
| `2` (`0x02`) | `0x90254E76` | `+0x128` | `0x90076E70` | Secondary Action 02 | **UNPROVEN** |
| `3` (`0x03`) | `0x90254E44` | `+0x120` | `0x90076E6C` | Alternate Action 03 | **UNPROVEN** |
| `4` (`0x04`) | `0x90254E4C` | `+0x124` | `0x90076E6E` | Alternate Action 04 | **UNPROVEN** |
| `5` (`0x05`) | `0x90254E86` | `+0x130` | `0x90077512` | Nav Right / Enter Sublevel | **INFERENCE** |
| `6` (`0x06`) | `0x90254E8E` | `+0x134` | `0x90077540` | Nav Left / Return Level | **INFERENCE** |
| `7` (`0x07`) | `0x90254E96` | `+0x138` | `0x90077548` | Nav Up | **INFERENCE** |
| `8` (`0x08`) | `0x90254E9E` | `+0x13C` | `0x90077550` | Nav Down | **INFERENCE** |
| `9` (`0x09`) | `0x90254EA6` | `+0x140` | `0x900776EC` | Action 09 (Tab / Mode) | **UNPROVEN** |
| `10` (`0x0A`) | `0x90254EAE` | `+0x144` | `0x900776EE` | Action 10 (Tab / Mode) | **UNPROVEN** |
| `11` (`0x0B`) | `0x90254EB6` | `+0x148` | `0x900776F0` | Action 11 | **UNPROVEN** |
| `12` (`0x0C`) | `0x90254EBE` | `+0x14C` | `0x900777DE` | Action 12 | **UNPROVEN** |
| `13` (`0x0D`) | `0x90254EC6` | `+0x150` | `0x9007786A` | Action 13 | **UNPROVEN** |
| `14` (`0x0E`) | `0x90254ECE` | `+0x154` | `0x90255992` | Action 14 | **UNPROVEN** |
| `15` (`0x0F`) | `0x90254ED6` | `+0x158` | `0x900777E2` | Action 15 | **UNPROVEN** |
| `16` (`0x10`) | `0x90254EDE` | `+0x15C` | `0x900777E6` | Action 16 | **UNPROVEN** |
| `17` (`0x11`) | `0x90254EE6` | `+0x160` | `0x90077892` | Action 17 | **UNPROVEN** |
| `18` (`0x12`) | `0x90254EEE` | `+0x164` | `0x900778BA` | Action 18 | **UNPROVEN** |
| `19` (`0x13`) | `0x90254EF6` | `+0x168` | `0x9007796E` | Action 19 | **UNPROVEN** |
| `20` (`0x14`) | `0x90254EFE` | `+0x16C` | `0x90077866` | Action 20 | **UNPROVEN** |
| `21` (`0x15`) | `0x90254F06` | `+0x170` | `0x90077C7E` | Action 21 | **UNPROVEN** |
| `22` (`0x16`) | `0x90254F2E` | `+0x184` | `0x90077CF0` | Action 22 | **UNPROVEN** |
| `23` (`0x17`) | `0x90254E70` | — | None (Returns 1) | Ignored / Bypass | **PROVEN** |
| `24` (`0x18`) | `0x90254E70` | — | None (Returns 1) | Ignored / Bypass | **PROVEN** |
| `25` (`0x19`) | `0x90254F0E` | `+0x174` | `0x9007774E` | Action 25 | **UNPROVEN** |
| `26` (`0x1A`) | `0x90254F16` | `+0x178` | `0x90077796` | Action 26 | **UNPROVEN** |
| `27` (`0x1B`) | `0x90254F1E` | `+0x17C` | `0x90077CBC` | Menu Toggle / Back | **INFERENCE** |
| `28` (`0x1C`) | `0x90254F26` | `+0x180` | `0x90255AE6` | Action 28 | **UNPROVEN** |

---

## 5. CJAPFButtonEventParam Architecture & Lifetime Trace

The parameter object and the post-dispatch virtual call are observed, but the available slice does not prove that the call targets this object or frees it:

### Class Definition & VTable
- **RTTI Descriptor**: `0x91504930` (`21CJAPFButtonEventParam`, inherits `10CJAPFParam` at `0x9150493C`).
- **VTable**: `0x9152BD0C`
  - Slot `+0x00`: `0x9012B856` (getter/field accessor)
  - Slot `+0x04`: `0x9012B868` (virtual destructor)
  - Slot `+0x08`: `0x9012B882` (deleting destructor)
- **Object Layout**:
  - `+0x00`: `vtable` (`0x9152BD0C`)
  - `+0x04`: base class field
  - `+0x08`: event category flag
  - `+0x0C`: event value / key payload (`record.value_1`)

### Lifecycle Trace
1. **Factory call sites**:
   `CMENUController::ProcessEventQueue` directly calls `0x9012B82A` at
   `0x90032C18` and `0x90032C46`. That factory calls `0x900F01FC` with zero
   in `r0`, tests its returned pointer, and calls `0x9012B810` on the
   success path. This proves factory control flow, not an allocation strategy
   or object lifetime.

   `0x900F01FC` bounds-checks this selector against five cases and its
   selector-zero case loads immediate `0x2E` before calling another provider.
   The provider's type, the meaning of `0x2E`, and any pool/heap ownership are
   unproven; names such as “heap allocation” are intentionally avoided.
2. **Transfer**:
   Passed as argument `r2` into `CMENUController::InvokeButtonDispatch` (`0x90032AF8`).
   Forwarded unchanged to active view virtual method `OnEvent` (slot `+0x10`) $\rightarrow$ `OnButtonEvent` (slot `+0x40`) $\rightarrow$ `DispatchButtonAction` (slot `+0x44`).
3. **Observed post-dispatch virtual call**:
   Immediately after the dispatch call, code at `0x90032C80..0x90032C86` executes:
   ```arm
   0x90032C80: ldr   r0, [r4]            ; load vtable pointer
   0x90032C82: ldr   r1, [r0]            ; load vtable slot +0x00
   0x90032C84: mov   r0, r4              ; this = param
   0x90032C86: blx   r1                  ; indirect call
   ```
4. **Boundary**: this slice alone does not prove deallocation, a destructor call, or an async-lifetime rule. Treat cross-task pointer retention as **UNPROVEN** until the allocation, register provenance, and deleting-destructor paths are traced end-to-end.

---

## 6. Evidence Boundary Ledger

| Evidence Type | Scope | Classification | Verification Basis |
|---|---|---|---|
| TBB1 Table Layout | 59 entries, maps 0..58 to logical IDs | **PROVEN** | Byte anchor at `0x90252DAA`, bounds `0x90252D9E` |
| TBB2 Table Layout | 100 entries, routes 40 IDs to 0x9025A8D2 | **PROVEN** | Byte anchor at `0x9025A86E`, bounds `0x9025A860` |
| TBB3 Table Layout | 29 entries, routes IDs 1..28 to vtable slots | **PROVEN** | Byte anchor at `0x90254E1C`, bounds `0x90254E14` |
| VTable Target Handlers | All slots +0x120..+0x184 mapped to ROM | **PROVEN** | CMENUTopMenuView vtable `0x9151D874` |
| Post-dispatch virtual call | `blx [vtable + 0]` at `0x90032C86` | **PROVEN** | Code bytes at `0x90032C80..0x90032C86`; target/object identity unproven |
| GPIO Type-2 Decoupling | Routes via 0x10319D2C, not UI TBB | **PROVEN** | Disassembly of `0x9011DD5C` and `0x9011E2B4` |
| Directional Navigation Roles | Slots +0x130..+0x13C as Right/Left/Up/Down | **INFERENCE** | Menu cursor traversal routines at 0x90077512 |
| Confirm Selection Role | Slot +0x12C as OK / Confirm | **INFERENCE** | Menu item execution tree at 0x90076FF4 |
| Menu Back / Exit Role | Slot +0x17C as Menu Toggle | **INFERENCE** | Menu hierarchy exit at 0x90077CBC |
| Physical Switch Pin Wiring | Mapping of raw codes 0..58 to GPIO pins | **UNPROVEN** | Not discoverable from ROM disassembly alone |
| Vendor Action Slots Semantics | Roles of slots +0x140..+0x184 | **UNPROVEN** | Internal state modifications without text strings |
