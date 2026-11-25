# Nikon D3300 C 1.02 — Static Extension Architecture Handoff

Complete static reverse engineering audit of Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base: `0x90020000`).

---

## 1. Unified Subsystem Architectural Graphs

### Graph 1: Unified Event & GPIO Transport Graph (Decoupled from UI)
```text
[Physical Switch / GPIO Pin 8]
             │
             ▼
[GPIO Pin 8 Debouncer 0x9011DDA8] (Driver 0x902742C2, state 0x1011FFB4)
             │
             ▼
[Forwarder Helper 0x9011E738] (Signal token 0x00000008)
             │
             ▼
[Persistent Dispatcher Signal 0x90027316] (RelayEventEx slot 3)
             │
             ▼
[Worker Dispatcher Loop 0x900271A4]
             │
             ▼
[Slot 3 Callback 0x9011DD5C]
             │ (Queries A32 gate 0x90369364 -> r0 in {0, 1})
             ▼
[Payload Sender 0x9011E756]
             │ (push {r0-r4, lr}; writes only byte0=0x02; bytes 1..15 = stack image)
             │ Outgoing first halfword: 0x0002
             ▼
[snd_mbf Veneer 0x9002EC0E] (Queue state 0x1011FFB4, mbfid at +0x18, timeout 100ms)
             │
             ▼
[Receiver Task Loop 0x9011DD88]
             │ (Pops 16-byte message from mbfid)
             ▼
[Type 2 Handler 0x9011E2B4]
             │
             ▼
[Dynamic RAM Dispatch Table 0x10319D2C + 0x18]
             │ (bx r1 to runtime-registered callback at [0x10319D2C + 0x1C])
             ▼
     [DYNAMIC BOUNDARY — TERMINATION OF STATIC TRACE]
```

> [!WARNING]
> **UNPROVEN Boundary**: The GPIO route via `0x9011DD5C` / `0x9011E756` creates outgoing halfword `0x0002`.
> At `COPNOQueueButton` (`0x9013DDCC`), halfword `0x0002` trips the range check (`2 - 4 = 0xFFFFFFFE >= 0x12`) and takes the straight-copy branch to `0x9013DE14`, completely bypassing the button TBB table (`0x9013DDE4`).
> Downstream receiver `0x9011DD88` dispatches type 2 via dynamic RAM callback table `0x10319D2C` (`+0x18`). Any connection between this GPIO path and the logical menu record pipeline (`0x9011E08E`, `0x902AE0DA`, `0x90252D9E`) or physical camera buttons (MENU/OK/INFO) is strictly **UNPROVEN**.

---

### Graph 2: Unified Logical UI & Active View Routing Graph
```text
[Host / Controller Event Record (<HHIII 16-byte record)]
             │
             ▼
[Record Reader 0x90032B86] (Checks event != 0xFFFF)
             │
             ▼
[TBB Table 1: Logical Translation 0x90252DAA] (59 entries, range check 0x90252D9E: cmp r1, #0x3B)
             │ Maps raw codes 0..58 to 40 logical IDs (1..28 and 33..44)
             │ Unmapped codes route to 0x90252E56 (bx lr, returns 0)
             ▼
[CJAPFButtonEventParam Instantiation 0x9012B82A]
             │ Allocated in ProcessEventQueue (0x90032C18/0x90032C46)
             │ param+0x00 = vtable (0x9152BD0C), param+0x0C = record.value_1
             ▼
[Active Screen Lookup 0x9003FF50] (Evaluates container [controller + 0x18] + 0x0C)
             │ Extracts active view pointer: r0 = *(uint32_t*)(entry + 8)
             ▼
[Virtual Invocation 0x90032B0E]
             │ Calls active view slot +0x10 (0x9025A85E) with (this, logical_id, param)
             ▼
[TBB Table 2: Screen Event Filter 0x9025A86E] (100 entries, range check 0x9025A860: cmp r1, #0x64)
             │ All 40 button logical IDs (1..28, 33..44) route to Target 0x9025A8D2
             │ Calls active view slot +0xC4 (PreEventHook at 0x9025A85A)
             │ Tail-calls active view slot +0x40 (OnButtonEvent at 0x9025AD54)
             ▼
[Screen Event Router (Slot +0x40 at 0x9025AD54)]
             │ Calls slot +0x54 (PreActionHook at 0x9025A770)
             │ Calls slot +0x44 (DispatchButtonAction at 0x90254DD4)
             ▼
[TBB Table 3: View Action Router 0x90254E1C] (29 entries, range check 0x90254E14: cmp r5, #0x1D)
             ├── Logical ID 0x01 -> Slot +0x12C (0x90076FF4) [INFERENCE: Confirm / OK / Select]
             ├── Logical ID 0x02 -> Slot +0x128 (0x90076E70) [UNPROVEN: Action 02]
             ├── Logical ID 0x03 -> Slot +0x120 (0x90076E6C) [UNPROVEN: Action 03]
             ├── Logical ID 0x04 -> Slot +0x124 (0x90076E6E) [UNPROVEN: Action 04]
             ├── Logical ID 0x05 -> Slot +0x130 (0x90077512) [INFERENCE: Nav Right / Sublevel]
             ├── Logical ID 0x06 -> Slot +0x134 (0x90077540) [INFERENCE: Nav Left / Return]
             ├── Logical ID 0x07 -> Slot +0x138 (0x90077548) [INFERENCE: Nav Up]
             ├── Logical ID 0x08 -> Slot +0x13C (0x90077550) [INFERENCE: Nav Down]
             ├── Logical ID 0x1B -> Slot +0x17C (0x90077CBC) [INFERENCE: Menu Toggle / Back]
             ├── Logical IDs 0, 23, 24 -> Target 0x90254E70 [PROVEN: Returns 1, bypasses slot call]
             └── Logical IDs 9..22, 25, 26, 28 -> Dedicated action slots +0x140..+0x184 [UNPROVEN]
             │
             ▼
[Synchronous EventParam Destruction (0x90032C80..0x90032C86)]
             │ Immediately upon return from InvokeButtonDispatch:
             │ ldr r0, [param]; ldr r1, [r0]; mov r0, param; blx r1
             │ Post-dispatch virtual call observed; object lifetime is UNPROVEN
```

---

### Graph 3: Unified OSD & Hardware Plane DMA Graph
```text
[UI Screens / Graphical Widgets (Audio Meter 0x9030CB30, AF Reticle, Text)]
             │
             ▼
[10-Byte Vector Primitive Appender 0x903111BC]
             │ Format: <HHHHBB (X:u16, Y:u16, W:u16, H:u16, Attr:u8, Byte09:unwritten)
             │ Stride: count * 5 << 1 (exactly 10 bytes)
             │ WARNING: ZERO internal capacity check; increments *count_ptr unconditionally
             ▼
[2D Viewport Clipping Engine 0x9030C9D0]
             │ Clamps (X, Y) against (clip_x, clip_y); recalculates W, H
             ▼
[COSDDControl Layer (VTable 0x91517260)]
             │
             ├── Slot +0x04: SubmitRectList (0x902EDE22)
             │   12-byte COSDCRectInfo array
             │   Plane bounds check: cmp r0, #7 (rejects if plane >= 7)
             │   Iterates over rect count r7
             │
             └── Slot +0x08: FlushDisplayPlanes (0x902EDE9A)
                 Verifies clipping at 0x9030C9D0
                 Passes plane update to downstream helper (hardware role unproven)
             ▼
[COSDCColorPalette (VTable 0x9151FBB0)]
             │ Slot +0x18 (0x902AFF78): loads [this + 8] pointing to 256-color LUT
             ▼
[Downstream Plane-Update Helper 0x902BC5F4]
             │ Reads display controller state at 0x10348F6C
             │ Reads active plane descriptor at 0x10123358
             │ Calls 0x902BC24E / 0x902BC314; transfer mechanism unproven
             │ VSYNC / vertical-refresh synchronization: UNPROVEN
```

---

### Graph 4: Unified Telemetry & PTP Photographic Engine Graph
```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│               NAMESPACE 1: Core Camera State Model (CINFOManager 0x103C92DC)            │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  CPTPCExposureTime (0x500D at 0x90300FF8) --+                                          │
│                                             |--> 0x902FD8F8 (Core Shutter Engine)      │
│  CPTPCShutterSpeed (0xD1A2 at 0x902E4534) --+          |                               │
│                                                        +--> GetProperty(0x066C, 1) [Step]
│                                                        +--> GetProperty(0x066D, 1) [Fine]
│                                                        +--> GetProperty(0x02C1, 4) [Mode]
│                                                        +--> 0x902FD542 [0x066B, 1]     │
│                                                        +--> 0x902FD4A0 [0x00BD, 0x00B9, 0x02D6]
│                                                        +--> Microsecond Table 0x914FE9FC
│                                                                                        │
│  CPTPCFnumber (0x5007 at 0x90301C14) ---------> 0x90301B50                             │
│                                                        +--> GetProperty(0x066E, 1)     │
│                                                        +--> F-number Table 0x914FED48  │
│                                                                                        │
│  CPTPCWhiteBalance (0x5005 at 0x90302864) ----> GetProperty(0x066A, 4)                 │
│                                                        +--> Mapping Table 0x914FF1EE   │
└────────────────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────────────────────────┐
│           NAMESPACE 2: Display Telemetry Buffer (CKRRCInfoDispDataGrp 0x914ED324)      │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  79 Properties (Indices 0..78, RAM 0x104B9044..0x104B9180, Size: 316 bytes)            │
│  Producer: Bulk memcpy 0x9019DE46 from SP+4 in CKRRCInfoDispData::Update (0x9019CD66)  │
│  Consumer Loop: 0x90169A28 (queried via 0x903513AC)                                    │
│                                                                                        │
│  PROVEN Properties:                                                                    │
│    • 0x0040 (Index 24): battery_power_source_status (consumed by CPTPCBatteryLevel)   │
│    • 0x0054 (Index 65): iso_control_sensitivity (consumed by CPTPCDevicePropISO)      │
│                                                                                        │
│  UNPROVEN Properties (Remaining 77 properties):                                        │
│    • 0x002D (Index 4): NOT queried by PTP exposure handlers (remains UNPROVEN)         │
│    • 0x005A, 0x005C: Gate internal CSTBLModelFacade branches; no format string proof   │
│    • All 77 remaining properties have semantic_name = null per strict evidence rules   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Confirmed Host-Side Integration Contracts

For non-destructive extension modules running in the host simulator or host tooling:

1. **Logical Event Injection Contract (`core/dml_ui_logical_event.*`)**:
   - Simulator components consume verified logical IDs: `0x01` (Select/OK), `0x05..0x08` (Nav 4-way), `0x1B` (Menu/Back).
   - No assumptions about physical button wiring or GPIO Pin 8 are made.

2. **OSD host-model contract (`core/dml_draw_buffer.*`)**:
   - Uses a 10-byte record layout inspired by the stores at `0x903111BC`; this
     is not an established camera ABI or submission format.
   - Host bounds checking is enforced in the simulator only.
   - The simulator's ARGB buffer, dimensions, stride, selectors and clipping
     are not authorisation to write a camera display plane.

3. **Telemetry Observation Contract (`core/dml_camera_status.*`)**:
   - Reads verified battery power status (`0x0040`) and ISO sensitivity (`0x0054`).
   - Uses core model properties (`0x066C`, `0x066E`, `0x066A`) for shutter, aperture, and white balance without conflating them with InfoDisp display fields.

4. **EventParam Lifetime Contract**:
   - Lifetime of `CJAPFButtonEventParam` across asynchronous task boundaries is UNPROVEN.
   - All event values (`param + 0x0C`) must be copied immediately into module state.

---

## 3. Explicitly Forbidden & Unproven Runtime Actions

> [!CAUTION]
> The following operations are strictly forbidden on physical camera hardware:

1. **Do NOT inject synthetic events into the OS message buffer (`snd_mbf`)**:
   - `[0x1011FFB4 + 0x18]` is a T-Kernel RTOS queue with strict 100 ms timeouts.
   - Uncoordinated packet injection risks deadlocking input task `0x9011DD88` or triggering watchdog resets (`0x90226068`).

2. **Do NOT perform direct writes to display planes without hardware sync**:
   - The static trace reaches `COSDDControl` and a downstream plane-update helper; hardware-controller ownership is UNPROVEN.
   - Effects of asynchronous RAM writes are UNPROVEN without runtime evidence.

3. **Do NOT assert GPIO Pin 8 is MENU, OK, or INFO**:
   - Pin 8 triggers persistent callback `0x9011DD5C`, which sends outgoing halfword `0x0002`.
   - `0x0002` bypasses the button TBB and routes through dynamic RAM dispatch table `0x10319D2C + 0x18`.
   - The connection to physical buttons or logical menu IDs remains completely UNPROVEN.

4. **Do NOT treat OSD primitive byte `+0x09` as generic padding or reserved**:
   - Byte `+0x09` is not written by `0x903111BC`. Its consumer semantics remain UNPROVEN.

5. **Do NOT fabricate names for the 77 unproven InfoDisp properties**:
   - Table `0x914ED324` ordering does not prove property meaning.
   - Only properties with unbroken chains to standard format strings, APEX tables, or PTP serializers may be named.

---

## 4. Hardware Proof Checklist

Before deploying any experimental payload to physical hardware, the following hardware proofs must be obtained using physical test equipment:

- [ ] **Hardware Pin 8 Mapping**: Connect a logic analyzer to Expeed 4 GPIO Pin 8; press each physical camera button (MENU, OK, INFO, D-pad) and record which button toggles the pin.
- [ ] **RAM Table 0x10319D2C Post-Boot Dump**: Read RAM at `0x10319D2C` after camera initialization to identify the runtime-registered function pointer at offset `+0x18` (Channel 2 handler).
- [ ] **Display VSYNC Interrupt Timing**: Capture the vertical blanking interval interrupt handler to identify safe display plane switching windows.
- [ ] **Watchdog Refresh Rate**: Verify the kick interval of watchdog service `0x90226068` to ensure custom extensions do not starve the watchdog timer.
- [ ] **OSD Primitive Byte +0x09 Consumer**: Trace hardware rasterizer register writes to determine if byte `+0x09` is consumed by the hardware display plane blitter.

---

## 5. Automated Verification & Regression Suite

The repository maintains 141 automated tests verifying all static anchors, tables, and boundaries:

```bash
# Fast audit suite (40 tests against firmware binary)
D3300_FIRMWARE=/path/to/b860_102.bin make audit-fast

# Full regression suite (141 tests covering C modules, firmware anchors, and evidence integrity)
D3300_FIRMWARE=/path/to/b860_102.bin make test

# SHA256 integrity verification across 137 repository artifacts
shasum -a 256 -c SHA256SUMS
```
