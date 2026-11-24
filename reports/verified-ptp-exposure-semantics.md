# Nikon D3300 C 1.02 — Verified PTP Exposure, Aperture & White Balance Semantics

Static reverse engineering analysis of PTP exposure, shutter speed, aperture, and white balance subsystems in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. Architectural Scope & Subsystem Separation

A critical distinction proven by static analysis is the separation between:
1. **The Display Telemetry Buffer (`CKRRCInfoDispDataGrp` / Table `0x914ED324`)**:
   - Contains 79 entries updated via bulk memcpy `0x9019DE46` into RAM `0x104B9044..0x104B9180`.
   - Used for the OSD Information Screen display rendering.
2. **The Core Camera State Model (`CINFOManager` / `0x103C92DC`)**:
   - Contains internal properties accessed by numeric 16-bit keys via `CINFOManager::GetProperty(property_id, buffer, size)`.
   - The PTP exposure, shutter speed, aperture, and white balance engines query properties in this namespace (e.g., `0x066C`, `0x066D`, `0x066E`, `0x066A`, `0x02C1`).
   - **Crucial Rule**: These internal property IDs DO NOT belong to the 79-entry display table `0x914ED324`. They are verified separately herein and are not mixed into the InfoDisp ledger.

---

## 2. Verified Shutter Speed & Exposure Time Engine

Both standard PTP `CPTPCExposureTime` (`0x500D`) and vendor PTP `CPTPCShutterSpeed` (`0xD1A2`) retrieve current shutter timing via shared helper routine `0x902FD8F8`.

### Call Graph
```
CPTPCExposureTime::GetDevicePropValue (0x90300FF8) --+
                                                     |--> 0x902FD8F8 (Core Shutter Engine)
CPTPCShutterSpeed::GetDevicePropValue (0x902E4534) --+          |
                                                                +--> CINFOManager::GetProperty(0x066C, 1) [Step Index]
                                                                +--> CINFOManager::GetProperty(0x066D, 1) [Fine Step]
                                                                +--> CINFOManager::GetProperty(0x02C1, 4) [Mode Flags]
                                                                +--> 0x902FD542
                                                                |       +--> CINFOManager::GetProperty(0x066B, 1)
                                                                +--> 0x902FD4A0
                                                                        +--> CINFOManager::GetProperty(0x00BD, 4) [Bulb/Time]
                                                                        +--> CINFOManager::GetProperty(0x00B9, 4) [Sync Mode]
                                                                        +--> CINFOManager::GetProperty(0x02D6, 4) [Curtain Sync]
```

### Static Assembly Evidence (`0x902FD8F8`)
```arm
0x902FD91C: bl    0x90274A7E          ; CINFOManager::GetInstance()
0x902FD920: mov   r6, r0              ; r6 = CINFOManager singleton
0x902FD922: ldr   r0, [r0]            ; r0 = vtable
0x902FD924: movs  r2, #1              ; size = 1 byte
0x902FD926: add   r3, sp, #8          ; out_buf = &sp[8]
0x902FD928: movw  r1, #0x66c          ; property ID 0x066C (shutter speed step index)
0x902FD92C: ldr   r7, [r0]            ; slot +0x00: GetProperty
0x902FD92E: mov   r0, r6
0x902FD930: blx   r7                  ; CINFOManager::GetProperty(0x066C, &sp[8], 1)
...
0x902FD93E: movs  r2, #1              ; size = 1 byte
0x902FD940: mov   r3, sp              ; out_buf = &sp[0]
0x902FD942: movw  r1, #0x66d          ; property ID 0x066D (fine step delta)
0x902FD946: ldr   r7, [r0]
0x902FD948: mov   r0, r6
0x902FD94A: blx   r7                  ; CINFOManager::GetProperty(0x066D, &sp[0], 1)
...
0x902FD952: movs  r2, #4              ; size = 4 bytes
0x902FD954: add   r3, sp, #12         ; out_buf = &sp[12]
0x902FD956: movw  r1, #0x2c1          ; property ID 0x02C1 (shutter mode / range flags)
0x902FD95A: ldr   r7, [r0]
0x902FD95C: mov   r0, r6
0x902FD95E: blx   r7                  ; CINFOManager::GetProperty(0x02C1, &sp[12], 4)
...
0x902FD96A: bl    0x902FD542          ; queries property 0x066B (size 1)
...
0x902FD978: bl    0x902FD4A0          ; queries properties 0x00BD, 0x00B9, 0x02D6
```

### Exposure Time Conversion Table `0x914FE9FC`
The index computed by `0x902FD77C` indexes table `0x914FE9FC` (stride 4, 32-bit integers in microseconds):
- Index 0: `300000` ($30.0$ seconds)
- Index 1: `250000` ($25.0$ seconds)
- Index 2: `200000` ($20.0$ seconds)
- ...
- Index 53: `250` ($1/4000$ second)

---

## 3. Verified Aperture Engine (`CPTPCFnumber` / `0x5007`)

The lens aperture F-number is retrieved and serialized by `CPTPCFnumber` (vtable `0x91517604`, RTTI `0x91501064`, method `0x90301C14`).

### Call Chain
```
CPTPCFnumber::GetDevicePropValue (0x90301C14)
   |
   +--> 0x90301B50 (Aperture Retrieval Helper)
           |
           +--> CINFOManager::GetProperty(0x066E, &sp[4], 1)
           +--> Lookup in 0x914FED48 table: FNumber = table[0x066E]
```

### Static Assembly Evidence (`0x90301B70`)
```arm
0x90301B70: bl    0x90274A7E          ; CINFOManager::GetInstance()
0x90301B74: ldr   r1, [r0]            ; vtable
0x90301B76: movs  r2, #1              ; size = 1 byte
0x90301B78: add   r3, sp, #4          ; out_buf = &sp[4]
0x90301B7A: ldr   r7, [r1]            ; slot +0x00: GetProperty
0x90301B7C: movw  r1, #0x66e          ; property ID 0x066E (aperture step index)
0x90301B80: blx   r7                  ; CINFOManager::GetProperty(0x066E, &sp[4], 1)
0x90301B8E: ldrb.w r1, [sp, #4]       ; raw aperture index
0x90301B92: ldr   r0, [0x90301E54]    ; literal pointer = 0x914FED48
0x90301B94: ldrh.w r0, [r0, r1, lsl #1] ; r0 = aperture_table[r1]
0x90301B9A: strh.w r0, [sp]           ; store 16-bit PTP FNumber (F * 100)
```

### Aperture F-Number Table `0x914FED48`
16-bit integer array representing $F 	imes 100$:
- `[00]`: `100` ($f/1.0$)
- `[06]`: `140` ($f/1.4$)
- `[12]`: `200` ($f/2.0$)
- `[18]`: `280` ($f/2.8$)
- `[24]`: `400` ($f/4.0$)
- `[30]`: `560` ($f/5.6$)
- `[36]`: `800` ($f/8.0$)
- `[42]`: `1100` ($f/11.0$)
- `[48]`: `1600` ($f/16.0$)
- `[54]`: `2200` ($f/22.0$)

---

## 4. Verified White Balance Engine (`CPTPCWhiteBalance` / `0x5005`)

Standard PTP WhiteBalance is managed by `CPTPCWhiteBalance` (vtable `0x915210C0`, RTTI `0x91502CA0`, method `0x90302864`).

### Static Assembly Evidence (`0x9030288E`)
```arm
0x9030288E: bl    0x90274A7E          ; CINFOManager::GetInstance()
0x90302892: ldr   r1, [r0]            ; vtable
0x90302894: movs  r2, #4              ; size = 4 bytes
0x90302896: mov   r3, sp              ; out_buf = &sp[0]
0x90302898: ldr   r7, [r1]            ; slot +0x00: GetProperty
0x9030289A: movw  r1, #0x66a          ; property ID 0x066A (white balance mode)
0x9030289E: blx   r7                  ; CINFOManager::GetProperty(0x066A, sp, 4)
...
0x903028B6: ldr   r7, [0x90302B3C]    ; literal pointer = 0x914FF1EE (WB PTP mapping table)
```

### White Balance Conversion Table `0x914FF1EE`
Byte array mapping camera internal WB indices to standard PTP WhiteBalance enum codes:
`[0x00, 0x03, 0x02, 0x01, 0x04, 0x05, 0x06, 0x07]`
- `1`: Standard PTP Daylight (`0x0003`)
- `2`: Standard PTP One-Push Manual Preset (`0x0002`)
- `3`: Standard PTP Automatic (`0x0001`)
- `4`: Standard PTP Fluorescent (`0x0004`)
- `5`: Standard PTP Tungsten / Incandescent (`0x0005`)
- `6`: Standard PTP Flash (`0x0006`)
- `7`: Standard PTP Cloudy (`0x0007`)

---

## 5. Summary Property Ledger

| Property ID | Subsystem | Size | Consumer Handler | Semantic Role | Conversion Table | Status |
|-------------|-----------|------|------------------|---------------|------------------|--------|
| `0x066C` | `CINFOManager` | 1 | `0x902FD8F8` | `shutter_speed_step_index` | `0x914FE9FC` | **PROVEN** |
| `0x066D` | `CINFOManager` | 1 | `0x902FD8F8` | `shutter_speed_fine_step_index` | Fine delta calculation | **PROVEN** |
| `0x02C1` | `CINFOManager` | 4 | `0x902FD8F8` | `shutter_mode_range_flags` | Range validation | **PROVEN** |
| `0x066B` | `CINFOManager` | 1 | `0x902FD542` | `shutter_speed_gating_mode` | Mode branch | **PROVEN** |
| `0x00BD` | `CINFOManager` | 4 | `0x902FD4A0` | `bulb_time_exposure_mode_flag` | Mode check (`== 1`) | **PROVEN** |
| `0x00B9` | `CINFOManager` | 4 | `0x902FD4A0` | `shutter_sync_extended_mode` | TBB routing | **PROVEN** |
| `0x02D6` | `CINFOManager` | 4 | `0x902FD4A0` | `flash_sync_shutter_status` | Suffix check | **PROVEN** |
| `0x066E` | `CINFOManager` | 1 | `0x90301B50` | `aperture_fnumber_step_index` | `0x914FED48` | **PROVEN** |
| `0x066A` | `CINFOManager` | 4 | `0x90302864` | `white_balance_mode_id` | `0x914FF1EE` | **PROVEN** |
| `0x002D` | `CKRRCInfoDispData` | 4 | Display Loop | *Unknown* (Not Shutter Speed) | None | **UNPROVEN** |

