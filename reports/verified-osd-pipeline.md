# Nikon D3300 C 1.02 — Verified OSD Queue Path (Partial)

Static reverse-engineering observations for part of the OSD vector-record path in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`). Hardware/DMA/VSYNC semantics are not established by this ROM-only trace.

---

## 1. End-to-End Pipeline Architecture

The display rendering architecture connects high-level vector primitives to hardware plane refresh via the following pipeline:

```
┌────────────────────────────────────────────────────────┐
│            UI Renderers & Widget Generators            │
│  (Audio Meter 0x9030CB30, AF Reticle, Text, Icons)     │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│      10-Byte Vector Primitive Appender (0x903111BC)    │
│  Stride: count * 5 << 1 (10 bytes: X, Y, W, H, attr)   │
│  Byte +0x09: unwritten by 0x903111BC (UNPROVEN)        │
│  Bounds: ZERO internal check (caller-enforced)         │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│          2D Viewport Clipping Engine (0x9030C9D0)      │
│  Clamps (X, Y) against (clip_x, clip_y); resizes W, H  │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│           COSDDControl Layer (VTable 0x91517260)       │
│  • SubmitRectList (0x902EDE22): 12-byte COSDCRectInfo   │
│    Plane bounds check: cmp r0, #7 (rejects if >= 7)    │
│  • FlushDisplayPlanes (0x902EDE9A): verifies clipping   │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│           COSDCColorPalette (VTable 0x9151FBB0)        │
│  Slot +0x18 (0x902AFF78): 256-color LUT pointer        │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│       Downstream plane-update helper (0x902BC5F4)      │
│  Reads 0x10348F6C (display state) & 0x10123358 (plane) │
│  Calls 0x902BC24E and 0x902BC314                       │
│  Hardware transfer and timing semantics: UNPROVEN      │
└────────────────────────────────────────────────────────┘
```

---

## 2. 10-Byte Vector Primitive Appender (`0x903111BC`)

### Disassembly Analysis
```arm
0x903111bc: push   {r4, r5, r6, lr}
0x903111be: ldr    r0, [r2]              ; r0 = current count (*count_ptr)
0x903111c0: ldrh   r6, [r3]              ; r6 = rect.x (u16)
0x903111c2: ldr    r4, [sp, #0x10]       ; r4 = attribute/color index (5th arg)
0x903111c4: add.w  r5, r0, r0, lsl #2    ; r5 = count * 5
0x903111c8: adds   r0, r0, #1            ; count++
0x903111ca: strh.w r6, [r1, r5, lsl #1]  ; dest[count * 10 + 0] = X (u16)
0x903111ce: add.w  r1, r1, r5, lsl #1    ; r1 = &dest[count * 10]
0x903111d2: ldrh   r5, [r3, #2]          ; r5 = rect.y (u16)
0x903111d4: strh   r5, [r1, #2]          ; dest + 2 = Y (u16)
0x903111d6: ldrh   r5, [r3, #4]          ; r5 = rect.w (u16)
0x903111d8: strh   r5, [r1, #4]          ; dest + 4 = Width (u16)
0x903111da: ldrh   r3, [r3, #6]          ; r3 = rect.h (u16)
0x903111dc: strh   r3, [r1, #6]          ; dest + 6 = Height (u16)
0x903111de: strb   r4, [r1, #8]          ; dest + 8 = Color/Attribute (u8)
0x903111e0: str    r0, [r2]              ; *count_ptr = count + 1
0x903111e2: pop    {r4, r5, r6, pc}      ; return
```

### Stride & Field Layout
- **Stride**: `(count * 5) << 1` = exactly 10 bytes per primitive.
- **Offset `+0x00` (2 bytes, `u16`)**: X coordinate on screen plane.
- **Offset `+0x02` (2 bytes, `u16`)**: Y coordinate on screen plane.
- **Offset `+0x04` (2 bytes, `u16`)**: Width in pixels.
- **Offset `+0x06` (2 bytes, `u16`)**: Height in pixels.
- **Offset `+0x08` (1 byte, `u8`)**: Color / Palette index.
- **Offset `+0x09` (1 byte, `u8`)**: **NOT WRITTEN by `0x903111BC`**. Only `strb r4, [r1, #8]` is executed. Its consumer semantics remain strictly **UNPROVEN**.

### Critical Bounds Finding
`0x903111BC` performs **ZERO internal capacity or buffer limit checks**. It unconditionally increments `*count_ptr` and writes into memory. Buffer bounds must be enforced by calling modules.

---

## 3. 2D Rectangular Viewport Clipping (`0x9030C9D0`)

Takes parameters `(r1 = &rect, r2 = clip_x, r3 = clip_y, [sp, 0x14] = clip_w, [sp, 0x18] = clip_h)`:
1. Clamps `rect.x`: if `X < clip_x`, `Width -= (clip_x - X); X = clip_x`.
2. Clamps `rect.y`: if `Y < clip_y`, `Height -= (clip_y - Y); Y = clip_y`.
3. Clamps right edge: if `X + Width > clip_x + clip_w`, `Width = (clip_x + clip_w) - X`.
4. Clamps bottom edge: if `Y + Height > clip_y + clip_h`, `Height = (clip_y + clip_h) - Y`.
5. If `Width <= 0` or `Height <= 0`, returns false without updating the rectangle.

---

## 4. COSDDControl & Batching Architecture

Class `COSDDControl` (RTTI `0x91500F70`, vtable `0x91517260`):

- **Slot `+0x04` (`SubmitRectList` at `0x902EDE22`)**:
  - Processes an array of `COSDCRectInfo` structures (stride 12 bytes).
  - Enforces plane bounds:
    ```arm
    0x902EDE56: ldrb   r0, [r1, #9]        ; load plane_id
    0x902EDE58: cmp    r0, #7              ; plane < 7
    0x902EDE5A: bhs    0x902ede94          ; reject if plane >= 7
    ```
  - Loops over element count in `r7` (`adds r5, #1; cmp r5, r7; blt ...`).

- **Slot `+0x08` (`FlushDisplayPlanes` at `0x902EDE9A`)**:
  - Calls clipping routine `0x9030C9D0` to ensure no out-of-bounds regions are submitted.
  - Passes plane update records down to `0x902BC5F4` (downstream helper; hardware role unproven).

---

## 5. Palette & Color Lookup (`COSDCColorPalette`)

Class `COSDCColorPalette` (RTTI `0x915029F8`, vtable `0x9151FBB0`):
- **Slot `+0x10` (`0x902AFF16`)**: Bind palette to hardware raster plane.
- **Slot `+0x14` (`0x902AFF50`)**: Reset / clear palette binding.
- **Slot `+0x18` (`0x902AFF78`)**: Returns pointer `[this + 8]`, which points to a 256-entry RGBA / YUV color lookup table.

---

## 6. Downstream Plane-Update Helper (`0x902BC5F4`)

- Reads hardware display controller state from `0x10348F6C`.
- Reads active display plane descriptor from `0x10123358`.
- Calls `0x902BC24E` and `0x902BC314`.
- Its hardware-transfer mechanism, register ownership, and any VSYNC/vertical-blank synchronization are **UNPROVEN** by the inspected static evidence.

---

## 7. Safety & Evidence Ledger

| Pipeline Element | Address / Symbol | Classification | Evidence Basis |
|---|---|---|---|
| Primitive Appender | `0x903111BC` | **PROVEN** | Disassembly of `0x903111BC`, 10-byte stride `count * 5 << 1` |
| Primitive Stride | 10 bytes (`<HHHHBB`) | **PROVEN** | Stride math in `0x903111C4` and field stores `0x903111CA..E0` |
| Unwritten Byte | Offset `+0x09` | **UNPROVEN** | Never written by `0x903111BC`; hardware semantics unknown |
| No Appender Bounds Check | `0x903111BC` | **PROVEN** | Unconditional count increment in `0x903111C8` |
| Viewport Clipping | `0x9030C9D0` | **PROVEN** | Disassembly of clamp logic in `0x9030C9D0..0x9030CA3C` |
| COSDDControl Plane Limit | `plane < 7` | **PROVEN** | `cmp r0, #7; bhs 0x902EDE94` in `0x902EDE58` |
| Downstream-helper call | `0x902BC5F4` | **PROVEN** | Called by `FlushDisplayPlanes` (`0x902EDF2C`) |
| Palette Table Pointer | `0x902AFF78` | **PROVEN** | Slot `+0x18` loads `[this + 8]` in `COSDCColorPalette` |
| Runtime Injection Feasibility | Custom RAM writes | **UNPROVEN** | No runtime safety conclusion follows from this static trace |
