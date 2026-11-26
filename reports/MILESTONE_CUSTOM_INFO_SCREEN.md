# Milestone: host-simulated custom information screen

The current safe milestone is a custom information screen in the host
simulator. It demonstrates menu composition, state-provider interfaces and
module boundaries without claiming a usable camera display or input ABI.

## Verified firmware evidence

- InfoDisp property `0x0040` reaches the battery display source.
- InfoDisp property `0x0054` reaches the ISO-step display path.
- Core InfoDisp values and PTP-facing values use distinct conversion paths.
- Raw selectors `0x0B` and `0x0C` map to logical values `0x1B` and `1` in one
  verified selector function. Their physical button identities are unknown.
- `17CKRRCCameraStatus` belongs to the firmware-update/recovery subsystem; its
  `+0x84` field is an internal list pointer, not exposure status.

## Host-only components

- `DmlCameraStatus` is an abstraction populated by simulator fixtures.
- The 720x480 ARGB8888 draw buffer is a simulator format.
- The T-Kernel wrappers model lifecycle behavior for tests; they are not a
  recovered Nikon ABI.
- No camera task suspension, framebuffer address, VSYNC call, physical button
  hook or live property subscriber has been established.

## Completion criteria

1. Keep rendering and module behavior deterministic under host tests.
2. Expand the property map only with reproducible disassembly evidence.
3. Record every ROM-to-RAM boundary where static analysis stops.
4. Do not add a camera payload, patch image or installation path.

See `verified-runtime-boundaries.md` for the machine-checked limits.
