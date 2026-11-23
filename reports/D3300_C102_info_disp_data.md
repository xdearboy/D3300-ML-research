# Nikon D3300 C 1.02 — `CKRRCInfoDispData`

Firmware: decoded B image, SHA-256 `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base `0x90020000`.

## Identity and global instance

RTTI record `0x91502968` names `17CKRRCInfoDispData` through string `0x9150AFE3`. The primary vtable begins at `0x9151F740`.

Initialization code at `0x9013BDF8` loads the literal at `0x9013BED4`, which is RAM address `0x10492714`, and calls constructor `0x9019CC64`. This establishes a static instance without relying on address proximity.

## Snapshot ring

Constructor `0x9019CC64` clears object fields `+0x2B0`, `+0x2B4` and `+0x2B8`, installs the vtable and initializes an embedded synchronization object at `+0x2BC`.

Vtable method `0x9019CCBA` builds a ring:

- the first node is at `object+0x210` and the current node pointer is stored at `object+0x2B4`;
- the loop creates ten additional nodes, for eleven nodes total;
- each node stores a snapshot pointer at node offset `+0x0C`;
- snapshot pointers begin at `object+0x08` and advance by 52 bytes (`13 * 4`) per entry.

Vtable slot `+0x14` points to Thumb function `0x90350DDF`:

```arm
90350dde  ldr.w r0, [r0, #0x2b4]
90350de2  ldr   r0, [r0, #0x0c]
90350de4  bx    lr
```

It returns the current 52-byte snapshot.

## Decoder evidence

Worker `0x9019CD66` advances the ring and reads the current snapshot. Its first word is decoded into multiple normalized fields:

- bit 6;
- bit 5;
- bits 0..4 through a value mapping;
- bits 12..15 through another mapping;
- bit 8;
- bits 22..23;
- bit 21;
- bits 16..17;
- signed top byte through a larger dispatch table.

The routine continues through later snapshot words and produces a much larger information block. This behavior and the class name make `CKRRCInfoDispData` a stronger source candidate for values displayed by the stock shooting-information UI than `CKRRCCameraStatus`.

## Verified normalized-property map

The decoder does not leave its normalized values only on the stack. At `0x9019DE44` it copies `0x13C` bytes from `SP+4` to RAM `0x104B9044` (the destination literal is at `0x9019DF80`). This is 79 four-byte output slots.

A separate RTTI record at `0x915040E8` names `20CKRRCInfoDispDataGrp`; its vtable starts at `0x9152826C`. The first method, `0x903513AC`, reports 79 entries and returns table address `0x914ED324`. Each 12-byte entry contains:

1. a numeric property ID;
2. a declared value size;
3. a RAM address in the normalized output block.

All 79 entries cover `0x104B9044..0x104B917C` in an exact four-byte stride, matching the decoder's `0x13C`-byte copy. The first entries therefore establish these structural mappings:

| Output offset | RAM | Property ID | Size |
|---:|---:|---:|---:|
| `0x000` | `0x104B9044` | `0x002A` | 4 |
| `0x004` | `0x104B9048` | `0x002B` | 4 |
| `0x008` | `0x104B904C` | `0x002C` | 4 |
| `0x00C` | `0x104B9050` | `0x005A` | 4 |
| `0x010` | `0x104B9054` | `0x002D` | 4 |
| `0x014` | `0x104B9058` | `0x005D` | 4 |

The complete machine-readable map is in `verified-info-disp-property-map.json`.

This also rejects the earlier shortcut of treating every immediate `0x2D` as a route to `CKRRCInfoDispData`: other subsystems use the same small integer. Only the table-backed mapping above is established. Numeric property IDs are not PTP device-property codes or EXIF tags merely because their numbers happen to coincide.

No photographic names are assigned yet. The next verification step is to trace consumers of individual numeric property IDs to UI formatter/resource paths for shutter speed, aperture and ISO. This newly verified property table reduces that search from an anonymous 316-byte block to 79 explicit IDs.

## Reproduction

The same verifier used for the camera-status ring checks the global address, constructor, ring initializer, getter and vtable bytes:

```bash
python3 tools/d3300_verify_camera_status.py b860_102.bin \
  --verify --dump-json reports/verified-camera-status-ring.json

python3 tools/d3300_info_disp_map.py b860_102.bin \
  --verify --dump-json reports/verified-info-disp-property-map.json
```
