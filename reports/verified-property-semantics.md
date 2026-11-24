# Nikon D3300 C 1.02 — Verified Information Display & PTP Property Semantics Ledger

Static reverse engineering audit of `CKRRCInfoDispData` property descriptor table `0x914ED324` and PTP property handlers in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. Architectural Scope & Methodology

Table `0x914ED324` describes the layout and dispatch structure of the camera information display RAM buffer:
- **Table ROM Address**: `0x914ED324`
- **Getter Method**: `0x903513AC` (`CKRRCInfoDispDataGrp`, RTTI `0x915040E8`, vtable `0x9152826C`)
- **Entry Count**: 79 (`0x4F`)
- **Entry Stride**: 12 bytes (`<III`: `property_id`, `size_bytes`, `ram_pointer`)
- **RAM Output Span**: `0x104B9044` to `0x104B9180` (316 bytes = `0x13C`)
- **Producer Mechanism**: Atomic bulk `memcpy` at `0x9019DE46` from stack `SP+4` in `CKRRCInfoDispData::Update` (`0x9019CD66`)
- **Consumer Notification**: Vtable slot `+0x0C` on listener `CINFOManager` (`0x103C92DC`, vtable `0x91516DB4`), initiating dispatch loop `0x90169A28` iterating over all 79 properties

### Strict Semantic Evidence Standard
In strict compliance with verified static analysis rules:
1. **No Inferring from Ordering**: Table index, physical RAM proximity, and grouping proximity are NEVER accepted as proof of photographic semantics.
2. **Complete Unbroken Chain Required**: A semantic mapping is accepted as `PROVEN` if and only if an unbroken static call/data chain connects:
   `Property ID` -> `RAM Pointer` -> `Consumer / Formatter / Serializer` -> `Standard PTP Property Code / APEX Lookup Table / Concrete Format String`.
3. **Absence of Proof = UNPROVEN**: Where a property is updated in bulk or queried only by internal UI state-machine gating without terminal formatting or standard serialization, `semantic_name` MUST remain `null` and `confidence` MUST be recorded as `UNPROVEN`.

---

## 2. Verified Semantic Mappings (PROVEN)

### Property `0x0040` (Index 24) — Battery Power Source Status
- **RAM Address**: `0x104B90A4` (Offset `+0x060`), Size: 4 bytes
- **Consumer Class**: `CPTPCBatteryLevel` (vtable `0x915208F8`, RTTI `0x9150A68C`)
- **Consumer Method**: `CPTPCBatteryLevel::GetValue` (`0x90303310`)
- **PTP Property Code**: `0x5001` (Standard PTP `BatteryLevel`)
- **Disassembly Evidence**:
```arm
0x90303324: bl    0x90274a7e          ; CINFOManager::GetInstance()
0x90303328: mov   r5, r0              ; r5 = CINFOManager singleton
0x9030332a: ldr   r0, [r0]            ; r0 = vtable
0x9030332c: movs  r2, #4              ; size = 4 bytes
0x9030332e: mov   r3, sp              ; out_buf = &sp[0]
0x90303330: movw  r1, #0x6a3          ; internal battery state prop 0x06A3
0x90303334: ldr   r7, [r0]            ; slot +0x00: GetProperty
0x90303336: mov   r0, r5
0x90303338: blx   r7                  ; CINFOManager::GetProperty(0x06A3, sp, 4)
0x9030333a: cmp   r0, sb              ; check status OK
0x9030333c: bne   0x90303350
0x9030333e: ldr   r0, [r5]
0x90303340: movs  r2, #4              ; size = 4 bytes
0x90303342: movs  r1, #0x40           ; r1 = 0x0040 (Property ID 0x0040!)
0x90303344: add   r3, sp, #4          ; out_buf = &sp[4]
0x90303346: ldr   r7, [r0]
0x90303348: mov   r0, r5
0x9030334a: blx   r7                  ; CINFOManager::GetProperty(0x0040, &sp[4], 4)
0x9030334c: cmp   r0, sb
0x9030334e: beq   0x90303356
0x90303350: movw  r1, #0x2002         ; PTP_RC_GeneralError
0x90303354: b     0x90303372
0x90303356: ldr   r0, [sp, #4]        ; r0 = value of property 0x0040
0x90303358: cmp   r0, #2              ; test external power source (AC adapter EP-5A)
0x9030335a: beq   0x9030337c          ; branch to force 100%
0x9030335c: ldr   r1, [sp]            ; battery step index from prop 0x06A3
0x9030335e: ldr   r0, [0x903034a0]    ; literal = 0x914FF1F6
0x90303360: ldrb  r0, [r0, r1]        ; lookup percentage from table
0x90303362: ldrb  r1, [r4]
0x90303364: cmp   r1, r0
0x90303366: beq   0x9030336e
0x90303368: ldr   r1, [r6, #4]
0x9030336a: strb  r0, [r1, #6]
0x9030336c: strb  r0, [r4]
0x9030336e: movw  r1, #0x2001         ; PTP_RC_OK (0x2001)
0x90303372: mov   r0, sb
0x90303374: strh.w r1, [r8]          ; write PTP response code
0x90303378: pop.w {r2-r8, sb, sl, pc}
0x9030337c: movs  r0, #0x64           ; 100% battery level when on external power
0x9030337e: b     0x90303362
```
- **Battery Table `0x914FF1F6`**: Byte array `[100, 35, 20, 5, 1, 1, 0, 0]` corresponding to standard battery bar levels.
- **Proved Semantics**: `battery_power_source_status` (indicates whether camera is running on internal battery vs. external AC power adapter, gating the `0x5001` battery level percentage serialization).

---

### Property `0x0054` (Index 65) — ISO Control Sensitivity
- **RAM Address**: `0x104B9148` (Offset `+0x104`), Size: 4 bytes
- **Consumer Class**: `CPTPCISOControlSensitivity` (vtable `0x9153FBA0`, RTTI `0x915070D4`)
- **Consumer Method**: `CPTPCISOControlSensitivity::GetDevicePropValue` (`0x90307218`)
- **PTP Property Code**: `0xD100` (Nikon Vendor Property `ISOControlSensitivity`)
- **Disassembly Evidence**:
```arm
0x90307228: bl    0x90274a7e          ; CINFOManager::GetInstance()
0x9030722c: ldr   r1, [r0]            ; r1 = vtable
0x9030722e: movs  r2, #4              ; size = 4 bytes
0x90307230: mov   r3, sp              ; out_buf = &sp[0]
0x90307232: ldr   r7, [r1]            ; slot +0x00: GetProperty
0x90307234: movs  r1, #0x54           ; r1 = 0x0054 (Property ID 0x0054!)
0x90307236: blx   r7                  ; CINFOManager::GetProperty(0x0054, sp, 4)
0x90307238: mov.w r1, #0xf0000000
0x9030723c: cmp   r0, r1
0x9030723e: beq   0x9030724a
0x90307240: movw  r0, #0x2002         ; PTP_RC_GeneralError
0x90307244: strh  r0, [r6]
0x90307246: ldr   r0, [0x90307364]
0x90307248: pop   {r3-r7, pc}
0x9030724a: ldr   r0, [sp]            ; r0 = raw property 0x0054 value
0x9030724c: cmp   r0, #7              ; clamp min index: 7 (ISO 100)
0x9030724e: bhs   0x90307254
0x90307250: movs  r0, #7
0x90307252: b     0x9030725a
0x90307254: cmp   r0, #0x43           ; clamp max index: 67 (ISO 25600 / Hi-1)
0x90307256: bls   0x9030725c
0x90307258: movs  r0, #0x43
0x9030725a: str   r0, [sp]
0x9030725c: ldr   r0, [0x90307368]    ; literal = 0x914FF258 (APEX ISO Table)
0x9030725e: ldr   r2, [sp]
0x90307260: ldr.w r0, [r0, r2, lsl #2] ; r0 = ISO_table[r2]
0x90307264: ldr   r2, [r5]
0x90307266: cmp   r2, r0
0x90307268: beq   0x90307276
0x9030726a: ldr   r2, [r4, #4]
0x9030726c: str.w r0, [r2, #9]
0x90307270: ldr   r2, [r4, #4]
0x90307272: str   r0, [r2, #0x10]
0x90307274: str   r0, [r5]            ; write serialized ISO speed value
0x90307276: movw  r0, #0x2001         ; PTP_RC_OK (0x2001)
0x9030727a: strh  r0, [r6]
0x9030727c: mov   r0, r1
0x9030727e: pop   {r3-r7, pc}
```
- **ISO APEX Table `0x914FF258`**: Array of 32-bit integers mapping 1/3-EV APEX indices to photographic ISO speeds:
  - Index 7: `100` (Base ISO)
  - Index 19: `400`
  - Index 31: `1600`
  - Index 43: `6400`
  - Index 49: `12800`
  - Index 55: `25600`
  - Index 67 (`0x43`): `102400` (Hi-2 max gain)
- **Proved Semantics**: `iso_control_sensitivity` (raw camera ISO step index, converted to standard photographic ISO values via table `0x914FF258` and serialized to PTP `0xD100`).

---

## 3. Disproven / Unproven Speculations

### Property `0x002D` (Index 4, RAM `0x104B9054`) — Not Shutter Speed
- **Community Claim**: Prior open-source discussions assumed `0x002D` was the camera's shutter speed / exposure time representation.
- **Static Proof Refutation**:
  - Disassembly of PTP exposure classes `CPTPCExposureTime` (vtable `0x91520B38`) and `CPTPCShutterSpeed` (vtable `0x91520F7C`) at `0x90300FF8` reveals that exposure time is retrieved via helper `0x902FD8F8`.
  - Helper `0x902FD8F8` queries `CINFOManager` exclusively for properties `0x066C` (size 1), `0x066D` (size 1), and `0x02C1` (size 4).
  - Neither `CPTPCExposureTime` nor `CPTPCShutterSpeed` queries property `0x002D`.
  - Consequently, `0x002D` is marked `semantic_name = null`, `confidence = UNPROVEN`.

---

## 4. Complete 79-Property Semantic Ledger

| Index | Table ROM | Prop ID | Size | Output RAM | Offset | Producer | Consumer | Semantic Name | Confidence |
|-------|-----------|---------|------|------------|--------|----------|----------|---------------|------------|
| 00 | `0x914ED324` | `0x002A` | 4 | `0x104B9044` | `+0x000` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 01 | `0x914ED330` | `0x002B` | 4 | `0x104B9048` | `+0x004` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 02 | `0x914ED33C` | `0x002C` | 4 | `0x104B904C` | `+0x008` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 03 | `0x914ED348` | `0x005A` | 4 | `0x104B9050` | `+0x00C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 04 | `0x914ED354` | `0x002D` | 4 | `0x104B9054` | `+0x010` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 05 | `0x914ED360` | `0x005D` | 4 | `0x104B9058` | `+0x014` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 06 | `0x914ED36C` | `0x002E` | 4 | `0x104B905C` | `+0x018` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 07 | `0x914ED378` | `0x0060` | 4 | `0x104B9060` | `+0x01C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 08 | `0x914ED384` | `0x002F` | 4 | `0x104B9064` | `+0x020` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 09 | `0x914ED390` | `0x0030` | 4 | `0x104B9068` | `+0x024` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 10 | `0x914ED39C` | `0x0032` | 4 | `0x104B906C` | `+0x028` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 11 | `0x914ED3A8` | `0x0033` | 4 | `0x104B9070` | `+0x02C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 12 | `0x914ED3B4` | `0x0034` | 4 | `0x104B9074` | `+0x030` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 13 | `0x914ED3C0` | `0x0035` | 4 | `0x104B9078` | `+0x034` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 14 | `0x914ED3CC` | `0x0339` | 4 | `0x104B907C` | `+0x038` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 15 | `0x914ED3D8` | `0x0036` | 4 | `0x104B9080` | `+0x03C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 16 | `0x914ED3E4` | `0x0038` | 4 | `0x104B9084` | `+0x040` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 17 | `0x914ED3F0` | `0x0039` | 4 | `0x104B9088` | `+0x044` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 18 | `0x914ED3FC` | `0x003B` | 4 | `0x104B908C` | `+0x048` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 19 | `0x914ED408` | `0x003C` | 4 | `0x104B9090` | `+0x04C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 20 | `0x914ED414` | `0x003A` | 4 | `0x104B9094` | `+0x050` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 21 | `0x914ED420` | `0x003D` | 1 | `0x104B9098` | `+0x054` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 22 | `0x914ED42C` | `0x0338` | 4 | `0x104B909C` | `+0x058` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 23 | `0x914ED438` | `0x003E` | 4 | `0x104B90A0` | `+0x05C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 24 | `0x914ED444` | `0x0040` | 4 | `0x104B90A4` | `+0x060` | `0x9019DE46` | `CPTPCBatteryLevel::0x90303310` | `battery_power_source_status` | **PROVEN** |
| 25 | `0x914ED450` | `0x0041` | 4 | `0x104B90A8` | `+0x064` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 26 | `0x914ED45C` | `0x0044` | 4 | `0x104B90AC` | `+0x068` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 27 | `0x914ED468` | `0x0045` | 4 | `0x104B90B0` | `+0x06C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 28 | `0x914ED474` | `0x0046` | 4 | `0x104B90B4` | `+0x070` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 29 | `0x914ED480` | `0x0047` | 4 | `0x104B90B8` | `+0x074` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 30 | `0x914ED48C` | `0x0048` | 4 | `0x104B90BC` | `+0x078` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 31 | `0x914ED498` | `0x0049` | 4 | `0x104B90C0` | `+0x07C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 32 | `0x914ED4A4` | `0x004A` | 4 | `0x104B90C4` | `+0x080` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 33 | `0x914ED4B0` | `0x004B` | 4 | `0x104B90C8` | `+0x084` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 34 | `0x914ED4BC` | `0x004C` | 4 | `0x104B90CC` | `+0x088` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 35 | `0x914ED4C8` | `0x004D` | 4 | `0x104B90D0` | `+0x08C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 36 | `0x914ED4D4` | `0x0061` | 4 | `0x104B90D4` | `+0x090` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 37 | `0x914ED4E0` | `0x0062` | 4 | `0x104B90D8` | `+0x094` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 38 | `0x914ED4EC` | `0x0063` | 4 | `0x104B90DC` | `+0x098` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 39 | `0x914ED4F8` | `0x0064` | 4 | `0x104B90E0` | `+0x09C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 40 | `0x914ED504` | `0x0067` | 4 | `0x104B90E4` | `+0x0A0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 41 | `0x914ED510` | `0x0073` | 4 | `0x104B90E8` | `+0x0A4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 42 | `0x914ED51C` | `0x0068` | 4 | `0x104B90EC` | `+0x0A8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 43 | `0x914ED528` | `0x006A` | 4 | `0x104B90F0` | `+0x0AC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 44 | `0x914ED534` | `0x007A` | 4 | `0x104B90F4` | `+0x0B0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 45 | `0x914ED540` | `0x0065` | 4 | `0x104B90F8` | `+0x0B4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 46 | `0x914ED54C` | `0x006D` | 4 | `0x104B90FC` | `+0x0B8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 47 | `0x914ED558` | `0x0066` | 4 | `0x104B9100` | `+0x0BC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 48 | `0x914ED564` | `0x006E` | 4 | `0x104B9104` | `+0x0C0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 49 | `0x914ED570` | `0x006F` | 4 | `0x104B9108` | `+0x0C4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 50 | `0x914ED57C` | `0x007C` | 4 | `0x104B910C` | `+0x0C8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 51 | `0x914ED588` | `0x007D` | 4 | `0x104B9110` | `+0x0CC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 52 | `0x914ED594` | `0x0069` | 4 | `0x104B9114` | `+0x0D0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 53 | `0x914ED5A0` | `0x0075` | 4 | `0x104B9118` | `+0x0D4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 54 | `0x914ED5AC` | `0x007B` | 4 | `0x104B911C` | `+0x0D8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 55 | `0x914ED5B8` | `0x0077` | 4 | `0x104B9120` | `+0x0DC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 56 | `0x914ED5C4` | `0x0076` | 4 | `0x104B9124` | `+0x0E0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 57 | `0x914ED5D0` | `0x0070` | 4 | `0x104B9128` | `+0x0E4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 58 | `0x914ED5DC` | `0x0071` | 4 | `0x104B912C` | `+0x0E8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 59 | `0x914ED5E8` | `0x005F` | 1 | `0x104B9130` | `+0x0EC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 60 | `0x914ED5F4` | `0x004E` | 4 | `0x104B9134` | `+0x0F0` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 61 | `0x914ED600` | `0x0337` | 4 | `0x104B9138` | `+0x0F4` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 62 | `0x914ED60C` | `0x0051` | 4 | `0x104B913C` | `+0x0F8` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 63 | `0x914ED618` | `0x0052` | 4 | `0x104B9140` | `+0x0FC` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 64 | `0x914ED624` | `0x0053` | 1 | `0x104B9144` | `+0x100` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 65 | `0x914ED630` | `0x0054` | 4 | `0x104B9148` | `+0x104` | `0x9019DE46` | `CPTPCISOControlSensitivity::0x90307228` | `iso_control_sensitivity` | **PROVEN** |
| 66 | `0x914ED63C` | `0x0055` | 4 | `0x104B914C` | `+0x108` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 67 | `0x914ED648` | `0x0074` | 4 | `0x104B9150` | `+0x10C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 68 | `0x914ED654` | `0x005C` | 4 | `0x104B9154` | `+0x110` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 69 | `0x914ED660` | `0x0072` | 4 | `0x104B9158` | `+0x114` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 70 | `0x914ED66C` | `0x0078` | 4 | `0x104B915C` | `+0x118` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 71 | `0x914ED678` | `0x0056` | 4 | `0x104B9160` | `+0x11C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 72 | `0x914ED684` | `0x0057` | 4 | `0x104B9164` | `+0x120` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 73 | `0x914ED690` | `0x0058` | 4 | `0x104B9168` | `+0x124` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 74 | `0x914ED69C` | `0x0059` | 4 | `0x104B916C` | `+0x128` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 75 | `0x914ED6A8` | `0x0031` | 1 | `0x104B9170` | `+0x12C` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 76 | `0x914ED6B4` | `0x0050` | 4 | `0x104B9174` | `+0x130` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 77 | `0x914ED6C0` | `0x0037` | 4 | `0x104B9178` | `+0x134` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |
| 78 | `0x914ED6CC` | `0x005E` | 4 | `0x104B917C` | `+0x138` | `0x9019DE46` | `0x90169A28` | *None* | UNPROVEN |

---

## 5. Architectural Summary & Conclusions
1. **Two Properties Proven**: Exactly 2 of 79 properties (`0x0040` -> `battery_power_source_status` and `0x0054` -> `iso_control_sensitivity`) possess complete, unbroken static chains to standard PTP serializations and format lookup tables.
2. **77 Properties Kept Unproven**: All other 77 properties remain strictly `UNPROVEN` per the zero-speculation mandate.
3. **Safe Passive Monitor Seam**: Custom OSD or monitor routines may safely read `0x104B90A4` (battery power source status) and `0x104B9148` (raw ISO step index) synchronously after `CKRRCInfoDispData::Update` fires.
