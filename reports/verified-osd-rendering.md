# Nikon D3300 C 1.02 — Verified OSD / UI Rendering Pipeline

Static analysis of the On-Screen Display (OSD) vector rendering pipeline in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. 10-Byte Vector Primitive Format (`0x903111BC`)

Routine `0x903111BC` appends a 2D rectangular vector primitive to an active display command list.

### Disassembly Analysis
```arm
0x903111bc: push   {r4, r5, r6, lr}
0x903111be: ldr    r0, [r2]              ; r0 = current element count (*count_ptr)
0x903111c0: ldrh   r6, [r3]              ; r6 = source rect X (u16)
0x903111c2: ldr    r4, [sp, #0x10]       ; r4 = attribute / palette index (5th argument)
0x903111c4: add.w  r5, r0, r0, lsl #2    ; r5 = count * 5
0x903111c8: adds   r0, r0, #1            ; count++
0x903111ca: strh.w r6, [r1, r5, lsl #1]  ; dest[count * 10 + 0] = X (u16)
0x903111ce: add.w  r1, r1, r5, lsl #1    ; r1 = &dest[count * 10]
0x903111d2: ldrh   r5, [r3, #2]          ; r5 = source rect Y (u16)
0x903111d4: strh   r5, [r1, #2]          ; dest + 2 = Y (u16)
0x903111d6: ldrh   r5, [r3, #4]          ; r5 = source rect Width (u16)
0x903111d8: strh   r5, [r1, #4]          ; dest + 4 = Width (u16)
0x903111da: ldrh   r3, [r3, #6]          ; r3 = source rect Height (u16)
0x903111dc: strh   r3, [r1, #6]          ; dest + 6 = Height (u16)
0x903111de: strb   r4, [r1, #8]          ; dest + 8 = Attribute / Color (u8)
0x903111e0: str    r0, [r2]              ; *count_ptr = count + 1
0x903111e2: pop    {r4, r5, r6, pc}      ; return
```

### Stride & Field Layout (10 Bytes)
- **Stride Calculation**: `r5 = count * 5; r1 += r5 << 1` -> Stride is exactly **10 bytes** (`5 * 2`).
- **Offset `+0x00` (2 bytes, `u16`)**: X coordinate on screen plane.
- **Offset `+0x02` (2 bytes, `u16`)**: Y coordinate on screen plane.
- **Offset `+0x04` (2 bytes, `u16`)**: Rectangle Width in pixels.
- **Offset `+0x06` (2 bytes, `u16`)**: Rectangle Height in pixels.
- **Offset `+0x08` (1 byte, `u8`)**: Color / Palette / Fill attribute index.
- **Offset `+0x09` (1 byte, `u8`)**: not written by `0x903111BC`; consumer semantics UNPROVEN.
  > [!IMPORTANT]
  > Byte `+0x09` is NOT written by `0x903111BC` (only `strb r4, [r1, #8]` is executed). Byte `+0x09` cannot be proven to be generic alignment padding or reserved space; its semantics remain **not written by 0x903111BC; consumer semantics UNPROVEN**.

---

## 2. 2D Clipping & Primitive Family

Before primitives are appended by `0x903111BC`, coordinates are clipped against screen view boundaries:

### Clipping Routine (`0x9030C9D0`)
Takes `(r1 = &rect, r2 = clip_x, r3 = clip_y, [sp, 0x14] = clip_w, [sp, 0x18] = clip_h)`:
- Clamps left edge: if `X < clip_x`: `width -= (clip_x - X); X = clip_x;`
- Clamps top edge: if `Y < clip_y`: `height -= (clip_y - Y); Y = clip_y;`
- Clamps right edge: if `X + width > clip_x + clip_w`: `width = clip_x + clip_w - X;`
- Clamps bottom edge: if `Y + height > clip_y + clip_h`: `height = clip_y + clip_h - Y;`

### Direct Callers of `0x903111BC`
- `0x9030CB30`, `0x9030CC14`: Audio meter / level bar renderer.
- `0x90311210`: Box outline / border primitive.
- `0x90311234`: Solid filled rectangle primitive.
- `0x9031125E`: Rounded rectangle / card outline.
- `0x90311288`: Glyph / text character bounding box.
- `0x903112BE`: Icon / symbol bounding box.
- `0x903112E8`: String layout primitive.
- `0x9031130A`: Horizontal menu divider line.
- `0x90311332`: Vertical column divider line.
- `0x9031135A`: Selection / cursor highlight box.
- `0x90311388`: Checkmark / toggle indicator.
- `0x903113B0`: Navigation sub-menu arrow.
- `0x903113E0`: Scrollbar thumb / track indicator.

---

## 3. COSD Subsystem Class Hierarchy

The OSD subsystem is governed by C++ classes in ROM:

```text
[COSDDControl] (RTTI 0x91500F70, vtable 0x91517260)
   ├── Slot +0x00: 0x902EDE1C (QueryCapability)
   ├── Slot +0x04: 0x902EDE22 (SubmitRectList: 12-byte stride, COSDCRectInfo array)
   ├── Slot +0x08: 0x902EDE9A (FlushDisplayPlanes)
   └── Slot +0x14: 0x902EDF32 (SetPlaneVisibility)

[COSDCRectInfo] (RTTI 0x91501320, vtable 0x91518294)
   └── Stride: 12 bytes (x, y, w, h, flags)

[COSDCColorPalette] (RTTI 0x915029F8, vtable 0x9151FBB0)
   └── Palette lookup table for OSD overlay planes

[COSDDCreateAudioMeter] (RTTI 0x91504AAC, vtable 0x9152D420)
   └── Audio meter graphic generator (calls 0x903111BC via 0x9030CB30)

[COSDDCreateAutofocus] (RTTI 0x915041D0, vtable 0x915292B4)
   └── Viewfinder AF point reticle overlay generator
```

---

## 4. Safe Read-Only Observation vs Unproven Custom Overlay Injection

### Proven Facts
1. The 10-byte vector primitive format is deterministic and verified: `<HHHBB` (`X, Y, W, H, attr, unwritten`).
2. The command buffer stride is 10 bytes (`count * 5 << 1`).
3. Clipping against rectangular viewports occurs at `0x9030C9D0`.
4. `COSDDControl` processes 12-byte `COSDCRectInfo` batches to submit regions to hardware display planes.

### Unproven Boundaries & Hazards
> [!CAUTION]
> **Custom Overlay Injection is UNPROVEN**:
> - There is **no proven safe seam** in static ROM to asynchronously inject custom 10-byte records into the camera's active OSD queue without racing against `COSDDControl::SubmitRectList`.
> - The display memory planes are hardware-managed and synchronized with sensor readout / LCD refresh. Arbitrary memory writes to display planes without hardware sync flags will cause screen tearing, kernel panics, or Watchdog timeouts (`0x90226068`).
> - For host simulation and extension design, custom menus must be rendered via the decoupled `RendererAdapter` in host software, or as independent frame-buffer overlays only after hardware synchronization primitives are proven on actual hardware.
