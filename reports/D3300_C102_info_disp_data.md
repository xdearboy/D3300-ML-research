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

No photographic names are assigned yet. The next verification step is to trace consumers of the worker's normalized output and match individual offsets to UI formatter/resource paths for shutter speed, aperture and ISO.

## Reproduction

The same verifier used for the camera-status ring checks the global address, constructor, ring initializer, getter and vtable bytes:

```bash
python3 tools/d3300_verify_camera_status.py b860_102.bin \
  --verify --dump-json reports/verified-camera-status-ring.json
```
