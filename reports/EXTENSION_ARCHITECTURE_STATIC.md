# Nikon D3300 C 1.02 — Static Extension Architecture Handoff

Complete static reverse engineering audit of Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base: `0x90020000`).

---

## 1. Verified Subsystem Call Graphs

### A1. GPIO Type-2 Transport (Terminates at Dynamic RAM Boundary)
```text
[Physical Button / GPIO Pin 8] ──▶ [GPIO Debouncer 0x9011DDA8]
                                         │
                                         ▼
                               [Helper 0x9011E738]
                                         │ (RelayEventEx mask 0x08)
                                         ▼
                            [Persistent Callback 0x9011DD5C]
                                         │ (A32 Gate 0x90369364 -> r0 in {0, 1})
                                         ▼
                               [Sender 0x9011E756]
                                         │ (push {r0-r4,lr}; strb byte0=2; outgoing halfword 0x0002)
                                         ▼
                             [snd_mbf veneer 0x9002EC0E]
                                         │ (Queue state 0x1011FFB4)
                                         ▼
                            [Receiver Task Loop 0x9011DD88]
                                         │ (Type 2 handler 0x9011E2B4)
                                         ▼
                         [Dynamic RAM Callback Boundary 0x10319D2C]
                                         │
                                         ▼ (bx r1 to handler at [0x10319D2C + 0x18 + 0x04])
                                  [DYNAMIC BOUNDARY]
```

> [!WARNING]
> **UNPROVEN Boundary**: The GPIO route via `0x9011DD5C` / `0x9011E756` creates outgoing halfword `0x0002`.
> At `COPNOQueueButton` (`0x9013DDCC`), halfword `0x0002` trips the range check (`2 - 4 = 0xFFFFFFFE >= 0x12`) and takes the straight-copy branch to `0x9013DE14`, completely bypassing the button TBB table (`0x9013DDE4`).
> Furthermore, type-2 handler `0x9011E2B4` dispatches through dynamic RAM callback table `0x10319D2C` (`+0x18`). Any connection between this GPIO path and the logical menu record pipeline (`0x9011E08E`, `0x902AE0DA`, `0x90252D9E`) is **UNPROVEN**.
>
> In sender `0x9011E756`, `push {r0-r4, lr}` saves incoming registers, and only byte 0 is overwritten with `0x02`. Bytes 1..15 are the stack image of saved incoming registers (`r0..r3`); they cannot be assumed to be padding, uninitialized, or reserved; their semantics are **UNPROVEN**.

### A2. Proven Logical Menu Record & Active View Dispatch
```text
[Queue Record (<HHIII 16-byte record)]
              │
              ▼
[COPNOQueueButton 0x9013DDCC]
              │
              ▼
[CMENUController::Pump 0x90032CA4]
              │
              ▼
[Logical Translation 0x90252D9E] (TBB 59 entries: 0x0C->0x01, 0x0B->0x1B, 0x10->0x05)
              │
              ▼
[CJAPFButtonEventParam 0x9012B82A]
              │
              ▼
[Active Screen Resolution 0x9003FF50] (Fetches active CGUICView from entry list)
              │
              ▼
[CMENUTopMenuView::OnEvent 0x9025A85E] (Slot +0x10 -> Slot +0x40 -> Slot +0x44)
              │
              ▼
[Action Router TBB 0x90254E18]
              ├── Logical ID 0x01 -> Slot +0x12C (0x90076FF4) [inference: Confirm / Item Action]
              ├── Logical ID 0x05 -> Slot +0x130 (0x90077512) [inference: Nav Right]
              ├── Logical ID 0x06 -> Slot +0x134 (0x90077540) [inference: Nav Left]
              ├── Logical ID 0x07 -> Slot +0x138 (0x90077548) [inference: Nav Up]
              ├── Logical ID 0x08 -> Slot +0x13C (0x90077550) [inference: Nav Down]
              └── Logical ID 0x1B -> Slot +0x17C (0x90077CBD) [inference: Menu Toggle / Back]
```

### B. OSD / UI Vector Rendering Pipeline
```text
[UI Screen / Widget] ──(Coordinate Geometry)──▶ [Clipping 0x9030C9D0]
                                                       │ (Clamps to view bounds)
                                                       ▼
                                            [Primitive Appender 0x903111BC]
                                                       │ (10-byte record: X, Y, W, H, Attr, byte +0x09)
                                                       ▼
                                            [Command Buffer (Stride 10)]
                                                       │
                                                       ▼
                                            [COSDDControl::SubmitRectList 0x902EDE22]
                                                       │ (12-byte COSDCRectInfo array)
                                                       ▼
                                            [Display Hardware Planes]
```
- Offset `+0x09` (1 byte, `u8`): not written by `0x903111BC`; consumer semantics UNPROVEN.

### C. Information Display Telemetry Pipeline
```text
[Telemetry Sensors / State] ──▶ [CKRRCInfoDispData::Update 0x9019CD66]
                                          │ (Formats 79 fields on stack)
                                          ▼
                               [Bulk Memcpy 0x9019DE46] (One confirmed producer: 316 bytes to 0x104B9044)
                                          │
                                          ▼
                         [Notify Listener Slot +0x0C (0x9019DE5C)]
                                          │
                                          ▼
                         [CINFOManager::OnInfoDispUpdated 0x90166252]
                                          │
                                          ▼
                               [Consumer Loop 0x90169A28]
                                          │ (Queries 0x903513AC -> table 0x914ED324)
                                          ▼
                         [Dispatch to UI Widgets 0x9016976C]
```

---

## 2. Safe Read-Only Observation Seams

All extensions must access camera state strictly via non-destructive read-only observation seams:

| Observation Target | Seam Type | Memory / Code Address | Access Method | Safety Guarantee |
|--------------------|-----------|------------------------|---------------|------------------|
| **Active Screen Pointer** | RAM Pointer | `0x10123844` | Direct word load (`ldr r0, [0x10123844]`) | Read-only; zero mutation |
| **Active Screen Entry** | Object Container | `[CMENUController + 0x18] + 0x0C` | Evaluator `0x9003FF50` | Read-only; returns active `CGUICView*` |
| **Last Button Message** | State Mailbox | State at `0x1012090C`, accessor `0x9035128C` | Getter `0x9035128C(state, &dest)` | Read-only copy of 16-byte buffer |
| **Telemetry (79 Properties)**| RAM Buffer | `0x104B9044..0x104B9180` | Read word/halfword at `0x104B9044 + offset` | Synchronously written by bulk memcpy `0x9019DE46` (one confirmed producer) |
| **Persistent Dispatcher State**| RAM Table | `0x1000216C` (32 slots) | Read slot `[0x1000216C + token*8]` | Read-only table inspection |

---

## 3. Explicitly Forbidden & Unproven Runtime Actions

> [!CAUTION]
> The following actions are strictly unproven and MUST NOT be attempted without prior physical verification:

1. **Do NOT claim a verified chain from GPIO Pin 8 to logical menu IDs**:
   - Static analysis proves GPIO Pin 8 triggers persistent callback `0x9011DD5C`, which sends a type-2 message via `0x9011E756` with outgoing halfword `0x0002`.
   - At `COPNOQueueButton` (`0x9013DDCC`), halfword `0x0002` bypasses the button TBB table and takes the straight-copy branch `0x9013DE14`.
   - Downstream receiver `0x9011DD88` dispatches type 2 via handler `0x9011E2B4`, which branches via dynamic RAM table `0x10319D2C + 0x18`.
   - Linking this GPIO path to logical menu translation `0x90252D9E` or physical buttons (MENU/OK/INFO) without runtime RAM dumps or hardware logic analyzer traces is speculative and forbidden.

2. **Do NOT assume sender payload bytes 1..15 are padding, reserved, or uninitialized**:
   - `0x9011E756` executes `push {r0-r4, lr}` and then writes only `byte0 = 0x02` (`strb.w r0, [sp]`).
   - Bytes 1..15 are the stack image of saved incoming registers (`r0..r3`).
   - Their semantics are **UNPROVEN**.

3. **Do NOT treat OSD primitive byte `+0x09` as generic padding or reserved**:
   - `0x903111BC` writes 1 byte at offset `+0x08` (`strb r4, [r1, #8]`); byte `+0x09` is not written by `0x903111BC`.
   - Consumer semantics remain **UNPROVEN**. Do not label it reserved.

4. **Do NOT claim `0x9019DE46` is the "only" producer of InfoDisp properties**:
   - `0x9019DE46` is **one confirmed producer** (bulk memcpy of 316 bytes from stack).
   - Whether other writers or per-property formatters exist in unanalyzed code remains UNPROVEN until exhaustive static/runtime verification.

5. **Do NOT inject synthetic events into the OS message buffer (`snd_mbf`)**:
   - The message buffer `[0x1011FFB4 + 0x18]` is a T-Kernel RTOS kernel queue with strict timing requirements (`0x64` = 100 ms timeout).
   - Injecting uncoordinated packets risks deadlocking the input task (`0x9011DD88`) or triggering watchdog timeouts (`0x90226068`).

6. **Do NOT perform direct writes to display planes without hardware sync**:
   - Display planes are managed by `COSDDControl` and hardware DMA controllers. Asynchronous memory writes cause visual artifacts, frame buffer tearing, and display engine stalls.

---

## 4. Hardware Proof Checklist

Before deploying any experimental code onto physical hardware, the following proofs must be captured on physical camera hardware:

- [ ] **Hardware Pin 8 Mapping**: Connect a logic analyzer to Expeed 4 GPIO Pin 8; press each physical camera button (MENU, OK, INFO, D-pad) and record which physical button toggles the pin state.
- [ ] **RAM Table 0x10319D2C Runtime Snapshot**: Dump RAM at `0x10319D2C` after camera boot to identify the function pointer registered at offset `+0x18` (Channel 2 handler).
- [ ] **Display VSYNC Interrupt Timing**: Log the vertical blanking interval interrupt handler to identify safe display update windows.
- [ ] **Watchdog Refresh Rate**: Verify the kick interval of watchdog service `0x90226068` to ensure custom routines do not starve the watchdog.

---

## 5. Host Simulator & Tooling Roadmap

Because on-device payload execution carries brick risk, development proceeds via host simulation:

1. **Host Simulator (`sim/main.c`, `build/d3300_sim`)**:
   - Implements native C simulation of the verified D3300 UI state machine.
   - Accurately models `CMENUController`, `CMENUTopMenuView`, `COSDDControl`, and the 10-byte vector primitive pipeline.
   - Allows fully interactive menu testing, key navigation, and module development on macOS/Linux.

2. **Automated Test Suite (`make test`, `make audit-fast`)**:
   - `tests/test_input_chain.py`: verified input tests, GPIO halfword 0x0002, and TBB bypass verification.
   - `tests/test_menu_architecture.py`: verified menu hierarchy and slot routing tests.
   - `tests/test_osd_rendering.py`: verified OSD primitive tests with unwritten byte +0x09 checks.
   - `tests/test_property_consumers.py`: verified telemetry ledger and producer tests.
   - `tests/test_registration_callsites.py`: callback registration tests.
   - `tests/test_persistent_dispatcher.py`: dispatcher loop tests.
   - `tests/test_proven_tables.py`: ROM anchor tests.
   - Complete C unit tests: `test_menu`, `test_adapter`, `test_draw_buffer`, `test_tkernel`, `test_payload`, `test_info_screen`, `test_camera_status_hw`, `test_menu_screen`, `test_fuji_film_sim`, `test_photo_assist`, `test_mvp_pipeline`, `test_relay_snapshot`.
