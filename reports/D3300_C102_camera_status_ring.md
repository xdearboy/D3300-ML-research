# Nikon D3300 C 1.02 — `CKRRCCameraStatus` snapshot ring

Firmware: decoded B image, SHA-256 `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base `0x90020000`.

## Result

The object at RAM `0x10491580` is `17CKRRCCameraStatus`, but its bytes at `+0x8C..+0x98` are not direct shutter, aperture, ISO, battery or autofocus values. They are transition indicators derived by comparing two 8-byte bitfield snapshots.

The earlier `DmlHwCameraStatusRaw` photographic field assignment is therefore a host fixture, not a recovered camera layout.

## Ring construction

The constructor at `0x9019A340` installs vtable `0x9151F6F0` and clears words at `object+0x80` and `object+0x84` (`0x9019A34E..0x9019A352`).

The initializer at `0x9019A3AA`:

- stores `object+0x30` at `object+0x84`;
- builds linked nodes inside the object;
- places a snapshot pointer in every node at offset `+0x0C`;
- uses successive 8-byte areas beginning at `object+0x08` as snapshots.

This is internal storage owned by `CKRRCCameraStatus`, not a pointer to an exposure structure.

## Getter

Vtable slot `+0x14` contains Thumb pointer `0x90350DD1`. Its complete implementation is:

```arm
90350dd0  ldr.w r0, [r0, #0x84]
90350dd4  ldr   r0, [r0, #0x0c]
90350dd6  bx    lr
```

Thus the returned value is the snapshot pointer stored in the current ring node.

## Snapshot comparison

Worker `0x9019A4E8` advances the ring and loads:

- `R5 = [current_node + 0x0C]`;
- `R6 = [[current_node + 0x04] + 0x0C]`.

It then compares individual bits from the two 32-bit words in these snapshots. Examples:

| Object byte | Compared snapshot field | Observed output |
|---|---|---|
| `+0x8F` | word 0, bit 5 | `0` unchanged, `1/2` according to transition direction |
| `+0x90` | word 0, bit 4 | `0`, `1` or `2` |
| `+0x91` | word 0, bit 2 | `0`, `1` or `2` |
| `+0x8E` | word 0, bit 15 | transition indicator plus a downstream callback |
| `+0x93` | word 0, bit 13 | `0`, `1` or `2` |
| `+0x94` | word 0, bit 12 | `0`, `1` or `2` |
| `+0x95` | word 0, bit 11 | `0`, `1` or `2` |
| `+0x8C` | word 0, bit 9 | `0`, `1` or `2` |
| `+0x8D` | word 0, bit 10 | binary state derived from the current snapshot |
| `+0x96` | word 0, bit 22 | `0`, `1` or `2` |
| `+0x97` | word 0, bit 18 | `0`, `1` or `2` |
| `+0x98` | word 0, bit 14 | `0`, `1` or `2` plus a downstream callback |

The semantic names of these bits remain unknown. Assigning photographic meanings would require tracing the producer of each snapshot bit and its downstream consumers.

## Reproduction

```bash
python3 tools/d3300_verify_camera_status.py b860_102.bin \
  --verify --dump-json reports/verified-camera-status-ring.json
```

The verifier checks the firmware hash and exact instruction/vtable bytes used above. It does not infer semantics from numeric values.
