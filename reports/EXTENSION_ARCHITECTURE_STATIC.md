# Nikon D3300 C 1.02 — Static Extension Architecture Handoff

Complete static reverse engineering audit of Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base: `0x90020000`).

---

## 1. Verified Subsystem Call Graphs

### A. UI Button-Event Path
```text
[Physical Button / GPIO] ──(Hardware transport)──▶ [GPIO Debouncer 0x9011DDA8]
                                                           │
                                                           ▼
                                                 [Helper 0x9011E738]
                                                           │ (RelayEventEx mask 0x08)
                                                           ▼
                                              [Callback 0x9011DD5C]
                                                           │ (A32 Gate 0x90369364)
                                                           ▼
                                                [Sender 0x9011E756]
                                                           │ (Type 2, 16 bytes)
                                                           ▼
                                              [snd_mbf veneer 0x9002EC0E]
                                                           │
                                                           ▼
                                             [Receiver Task 0x9011DD88]
                                                           │ (Type 2 handler 0x9011E2B4)
                                                           ▼
                                           [RAM Callback Table 0x10319D2C]
                                                           │
                                                           ▼
                                            [Record Assembler 0x902AE122]
                                                           │ (<HHIII 16-byte record)
                                                           ▼
                                            [COPNOQueueButton 0x9013DDCC]
                                                           │
                                                           ▼
                                           [CMENUController::Pump 0x90032CA4]
                                                           │
                                                           ▼
                                           [Logical Translate 0x90252D9E]
                                                           │ (TBB 59 entries: 0x0C->1, 0x0B->0x1B, 0x10->5)
                                                           ▼
                                         [CJAPFButtonEventParam 0x9012B82A]
                                                           │
                                                           ▼
                                           [Active Screen Getter 0x9003FF50]
                                                           │ (Fetches active CGUICView from entry list)
                                                           ▼
                                         [CMENUTopMenuView::OnEvent 0x9025A85E]
                                                           │ (Slot +0x10 -> Slot +0x40 -> Slot +0x44)
                                                           ▼
                                         [Action Router TBB 0x90254E18]
                                                           ├── Code  1 (0x01) -> Slot +0x12C (0x90076FF4 Confirm)
                                                           ├── Code  5 (0x05) -> Slot +0x130 (0x90077512 Nav Right)
                                                           ├── Code  6 (0x06) -> Slot +0x134 (0x90077540 Nav Left)
                                                           ├── Code  7 (0x07) -> Slot +0x138 (0x90077548 Nav Up)
                                                           ├── Code  8 (0x08) -> Slot +0x13C (0x90077550 Nav Down)
                                                           └── Code 27 (0x1B) -> Slot +0x17C (0x90077CBD Menu Toggle)
```

### B. OSD / UI Vector Rendering Pipeline
```text
[UI Screen / Widget] ──(Coordinate Geometry)──▶ [Clipping 0x9030C9D0]
                                                       │ (Clamps to view bounds)
                                                       ▼
                                            [Primitive Appender 0x903111BC]
                                                       │ (10-byte record: X, Y, W, H, Attr, Unwritten)
                                                       ▼
                                            [Command Buffer (Stride 10)]
                                                       │
                                                       ▼
                                            [COSDDControl::SubmitRectList 0x902EDE22]
                                                       │ (12-byte COSDCRectInfo array)
                                                       ▼
                                            [Display Hardware Planes]
```

### C. Information Display Telemetry Pipeline
```text
[Telemetry Sensors / State] ──▶ [CKRRCInfoDispData::Update 0x9019CD66]
                                          │ (Formats 79 fields on stack)
                                          ▼
                               [Bulk Memcpy 0x9019DE46]
                                          │ (Copies 316 bytes to RAM 0x104B9044)
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
| **Telemetry (79 Properties)**| RAM Buffer | `0x104B9044..0x104B9180` | Read word/halfword at `0x104B9044 + offset` | Synchronously written by `0x9019DE46` |
| **Persistent Dispatcher State**| RAM Table | `0x1000216C` (32 slots) | Read slot `[0x1000216C + token*8]` | Read-only table inspection |

---

## 3. Explicitly Forbidden & Unproven Runtime Actions

> [!CAUTION]
> The following actions are strictly unproven and MUST NOT be attempted without prior physical verification:

1. **Do NOT label GPIO Pin 8 as MENU / OK / INFO**:
   - Static analysis proves GPIO Pin 8 triggers persistent callback `0x9011DD5C`, which sends a type-2 message to message buffer `0x1011FFB4`.
   - Receiver `0x9011DD88` invokes channel 2 from RAM table `0x10319D2C + 0x18`.
   - Table `0x10319D2C` is dynamically registered at runtime in uninitialized RAM. No static ROM table binds channel 2 to button codes `0x0B..0x15`.
   - Any physical button assignment without hardware logic analyzer traces is speculative and forbidden.

2. **Do NOT inject synthetic events into the OS message buffer (`snd_mbf`)**:
   - The message buffer `[0x1011FFB4 + 0x18]` is a T-Kernel RTOS kernel queue with strict timing requirements (`0x64` = 100 ms timeout).
   - Injecting uncoordinated packets risks deadlocking the input task (`0x9011DD88`) or triggering watchdog timeouts (`0x90226068`).

3. **Do NOT treat OSD primitive byte `+0x09` as generic padding**:
   - `0x903111BC` writes 1 byte at offset `+0x08` (`strb r4, [r1, #8]`); byte `+0x09` remains unwritten.
   - Its semantics in hardware graphics registers remain **UNPROVEN**.

4. **Do NOT perform direct writes to display planes without hardware sync**:
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
   - `tests/test_input_chain.py`: 23 verified input tests.
   - `tests/test_menu_architecture.py`: 8 verified menu hierarchy tests.
   - `tests/test_osd_rendering.py`: 4 verified OSD primitive tests.
   - `tests/test_property_consumers.py`: 5 verified telemetry ledger tests.
   - `tests/test_registration_callsites.py`: 8 callback registration tests.
   - `tests/test_persistent_dispatcher.py`: 8 dispatcher loop tests.
   - `tests/test_proven_tables.py`: 7 ROM anchor tests.
   - Complete C unit tests: `test_menu`, `test_adapter`, `test_draw_buffer`, `test_tkernel`, `test_payload`, `test_info_screen`, `test_camera_status_hw`, `test_menu_screen`, `test_fuji_film_sim`, `test_photo_assist`, `test_mvp_pipeline`, `test_relay_snapshot`.
