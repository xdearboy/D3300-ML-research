# Nikon D3300 C 1.02 Firmware Update & SD Card Diagnostic Hook Seams

## 1. Executive Summary

This report documents the reverse engineering of the Nikon D3300 C 1.02 firmware update process, SD card filesystem reader architecture, and internal diagnostic hooks discovered in `b860_102.bin`. 

All findings were obtained via static analysis of the decoded B-firmware image without hardware modifications or live flashing.

---

## 2. Firmware Update Mechanism (`CGPSCProcedure`)

### 2.1 File Candidates
The camera firmware update subsystem searches the SD card root directory (`/sd0/`) for three candidate image files:
- `/sd0/A.BIN` (Literal address: `0x90136ECC`): A-firmware / bootloader / power management supervisor.
- `/sd0/B.BIN` (Literal address: `0x90136ED8`): B-firmware / main operating system image.
- `/sd0/C.BIN` (Literal address: `0x90136EE4`): Distortion control / lens database package.

### 2.2 Invocation Seam (`0x90136DA4`)
The entry point is Method 37 of class `CGPSCProcedure` (`14CGPSCProcedure`, vtable at `0x915199B0`, method slot at `0x91519A44`):
```arm
90136da4: push.w  {r4, r5, r6, r7, r8, lr}
90136da8: mov     r4, r0                 ; this pointer
90136daa: mov     r5, r1                 ; session context
90136dac: adds    r0, r0, #4
90136dae: sub     sp, #0x20
90136db0: mov.w   r8, #0xf0000000
90136db4: movw    r1, #0x357
90136db8: bl      0x900e62ac
90136dbc: movs    r1, #5                 ; Command ID = 5 (Firmware Update Verification)
90136dbe: movs    r0, #10
90136dc0: adr     r2, #264               ; -> 0x90136ecc ("/sd0/A.BIN")
90136dc2: strb.w  r1, [sp]               ; sp[0] = 5
90136dc6: strd    r0, r2, [sp, #4]       ; sp[4] = 10, sp[8] = "/sd0/A.BIN"
90136dca: adr     r3, #268               ; -> 0x90136ed8 ("/sd0/B.BIN")
90136dcc: adr     r6, #276               ; -> 0x90136ee4 ("/sd0/C.BIN")
90136dce: movs    r7, #1
90136dd0: strd    r3, r6, [sp, #16]      ; sp[16] = "/sd0/B.BIN", sp[20] = "/sd0/C.BIN"
90136dd4: strb.w  r0, [sp, #0xc]         ; sp[12] = 10
90136dd8: mov     r0, sp
90136dda: str     r5, [sp, #0x18]        ; sp[24] = session context
90136ddc: strb.w  r7, [sp, #0x1c]        ; sp[28] = 1
90136de0: bl      0x902a3cd0             ; Dispatcher
```

### 2.3 Command Dispatch & Header Parser (`0x902A3CD0` -> `0x902A2950`)
- `0x902A3CD0` inspects `r0 = sp[0]` (value `5`).
- `tbb [pc, r0]` at `0x902A3CF4` jumps via entry 5 (`offset 0xAA`) to `0x902A3DA2`.
- `0x902A3DA2` invokes `0x902A2950`, passing the file descriptor structure.
- `0x902A2950` unpacks the file header using big-endian byte-swapping (`ubfx`, `orr`, `lsr` / `rev`) on the header fields:
  - `[r5 + 0x20]`: Header length / magic.
  - `[r5 + 0x24]`: Entry count / component count.
  - `[r5 + 0x28]`: Checksum / cryptographic digest pointer.
  - `[r5 + 0x2C]`: Payload segment size.
  - `[r5 + 0x30]`: Payload segment file offset.
  - `[r5 + 0x34]`: Target load address.

### 2.4 Container Header Format (`0x902E7EA4`)
The parser verifies the inner container structure:
- Root record identifier must be `'M'` (`0x4D`, checked at `0x902E7F10`).
- Sub-records are iterated in a loop up to `r8` sections; each entry must begin with `'S'` (`0x53`, checked at `0x902E7F36`).
- Once verified, control reaches thunk `0x90226040`, whose final target is runtime RAM state. The ROM call shape is consistent with task setup, but the exact T-Kernel service name and packet ABI are not proven by this image.

---

## 3. SD Card Diagnostic & Calibration Hooks

Static analysis reveals several hidden file hooks on the SD card mounted at `/sd0/`:

| Path | Address | Handler | Purpose |
|---|---|---|---|
| `/sd0/ReadStep.txt` | `0x902F5F0C` | `0x902F5B0E` | Factory calibration step script (parses decimal step IDs and sets DSP gains) |
| `/sd0/Param.csv` | `0x902F7758` | `0x902F7386` | CSV-formatted optical / lens distortion parameter table |
| `/sd0/Debug.log` | `0x91586772` | `0x91283928` | Kernel cyclic diagnostic logger destination buffer |
| `/sd0/LVRAW/LVRAW.BIN` | `0x9026F588` | `0x9026F1E6` | LiveView RAW frame injection test mode buffer |
| `/sd0/NCFL/NML_28A.ee` | `0x90136644` | `0x90136600` | Non-volatile EEPROM calibration backup image |

### 3.1 Factory Calibration Script (`/sd0/ReadStep.txt`)
At `0x902F5B0E`:
- Checks if `/sd0/ReadStep.txt` is present on the SD card.
- Reads up to 128 bytes into a stack buffer via `0x902B7CD2`.
- Parses numbers using a delimiter parser at `0x902B7D42` into integer fields:
  - Step 1 (`[sp + 0x84]`): Sensor readout mode (`0x902B70F0`).
  - Step 2 (`[sp + 0x88]`): Gain multiplier (`0x902B7114`).
  - Step 3 (`[sp + 0x8C]`): Shutter timing scaling factor written to `[r4 + 0x3C]`.

### 3.2 LiveView RAW Frame Injection (`/sd0/LVRAW/LVRAW.BIN`)
At `0x9026F1C2` (called from `CCIPBController` vtable entry at `0x9151AAA0`):
- Checks for file `/sd0/LVRAW/LVRAW.BIN`.
- If found, opens the stream via `vfunc [r1 + 0xE8]` and queries the payload length.
- Passes the stream data toward object field `[r4 + 0x794]` through `0x902765E8`. Static control flow does not establish that field as a DMA receive buffer or establish the resulting hardware behavior.

---

## 4. Architectural Takeaways for D3300-ML

1. **No firmware-flashing conclusion is authorized by this report**: RAM addresses in the analyzed image (`0x10000000..0x104A0000`) and file-service paths do not prove relocatable module hooks, an executable loader, or a safe camera execution path. Any in-memory-module design remains a hypothesis requiring independent ARM/Thumb, relocation, cache, privilege, and lifecycle evidence.
2. **Standard File Access**: The camera's file system service natively mounts the SD card as `/sd0/` and provides standard open/read/close facilities through `CJAFFSignalEvntPort` and `CGPSCProcedure`.
3. **OSD boundary**: a RAM data structure exists at `0x104A0868`; calling it a framebuffer descriptor table, assigning a pixel format, or mutating it is not justified by this report.
