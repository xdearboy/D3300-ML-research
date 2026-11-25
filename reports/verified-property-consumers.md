# Nikon D3300 C 1.02 — Verified Information Display Property Producer/Consumer Ledger

Static audit of `CKRRCInfoDispData` property descriptor table `0x914ED324` in Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. Architecture & Table Layout

Table `0x914ED324` contains **79 entries** describing the layout of the camera information display data buffer in RAM:
- **Table Address**: `0x914ED324`
- **Entry Count**: 79 (`0x4F`)
- **Entry Stride**: 12 bytes (`<III`: `property_id`, `size`, `ram_pointer`)
- **Base RAM Buffer**: `0x104B9044`
- **Total Span**: 316 bytes (`0x13C`, from `0x104B9044` to `0x104B9180`)

---

## 2. One Confirmed Producer: Bulk Memcpy (`0x9019DE46`)

One confirmed static producer that writes to the `0x104B9044..0x104B9180` RAM range is the bulk `memcpy` at `0x9019DE46`:

```arm
0x9019de44: add   r1, sp, #4           ; r1 = stack source buffer (&stack[4])
0x9019de46: ldr   r0, [0x9019df80]     ; r0 = destination RAM pointer (0x104B9044)
0x9019de48: mov.w r2, 0x13c            ; r2 = length (316 bytes, covers all 79 properties)
0x9019de4c: blx   0x90274b08           ; memcpy(0x104B9044, &stack[4], 316)
```

The observed call passes a 316-byte stack range to `memcpy` at `0x9019DE46`.
This proves one bulk write covering the table's represented output span. It
does **not** prove atomicity, update exclusion, the interpretation of every
source byte, or that no other writer exists.

---

## 3. Verified Consumer Chain: `CINFOManager` (`0x90169A28`)

Immediately following the bulk `memcpy`, `CKRRCInfoDispData` triggers the consumer pipeline:

```arm
0x9019de50: ldr.w r0, [sl, #0x2c0]     ; r0 = listener object (CINFOManager singleton 0x103C92DC)
0x9019de54: ldr   r1, [r0]             ; r1 = listener vtable (0x91516DB4)
0x9019de56: ldr   r2, [r1, #0xc]       ; r2 = vtable slot +0x0C (0x90166252)
0x9019de58: add.w r1, sl, #0x2bc       ; r1 = &this->info_disp_data_grp (vtable 0x9152826C)
0x9019de5c: blx   r2                   ; CINFOManager::OnInfoDispUpdated(grp)
```

In `CINFOManager::OnInfoDispUpdated` (`0x90166252`):
1. Calls `0x90169A28` passing the group object.
2. `0x90169A28` queries group vtable slot `+0x00` (`0x903513AC`), which returns count 79 and table base `0x914ED324`.
3. Iteration loop (`0x90169A48..0x90169A6E`):
   - Loops through `index = 0..78`.
   - Reads RAM address `table[index].ram_address` (`0x104B9044 + offset`).
   - Reads `property_id = table[index].property_id`.
   - Calls `0x9016976C` to dispatch the property value to the active UI widgets on the information screen.

---

## 4. Specific Property Audit

| Property ID | RAM Address | Output Offset | Size | Verified Producer | Verified Consumer Dispatcher | Direct Reader | Semantic Name | Semantic Confidence |
|-------------|-------------|---------------|------|-------------------|------------------------------|---------------|---------------|---------------------|
| `0x002A`    | `0x104B9044`| `0x000`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |
| `0x005A`    | `0x104B9048`| `0x004`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |
| `0x002B`    | `0x104B904C`| `0x008`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |
| `0x002C`    | `0x104B9050`| `0x00C`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |
| `0x002D`    | `0x104B9054`| `0x010`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |
| `0x005D`    | `0x104B90D0`| `0x08C`       | 4    | `0x9019DE46`      | `0x90169A28`                 | unknown       | None          | UNPROVEN            |

### Strict Consumer Rule
For all 79 properties:
- **Producer**: One confirmed producer at `0x9019DE46` (bulk memcpy); other writers UNPROVEN.
- **Consumer Dispatcher**: Confirmed as `0x90169A28` via `CINFOManager` slot `+0x0C`.
- **Direct Reader**: Marked `unknown` for any property lacking a dedicated direct call-chain.
- **Semantic Name**: Kept as `None / UNPROVEN` to avoid guessing or unverified claims.

---

## 5. Static boundary — not a runtime observation approval

The static trace identifies addresses and one bulk-copy producer. It does not
establish cache coherence, task ownership, update timing, camera access method,
or safety of reading live RAM. Consequently this repository makes **no**
camera-facing observation recommendation. In the host simulator, consumers may
model values using the descriptor offsets; on hardware, any observation would
require separate authorised runtime evidence and a safety review.
