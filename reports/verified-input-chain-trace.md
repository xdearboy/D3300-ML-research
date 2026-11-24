# Verified Static Audit: Input Pipeline & Menu Event Dispatch

Static reverse engineering trace of the input pipeline in Nikon D3300 firmware version C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`, ROM base `0x90020000`).

---

## 1. Executive Summary

This report establishes two decoupled static pipelines:
1. **GPIO Type-2 Transport**: Traces physical pin debouncing through T-Kernel message buffering and terminates at dynamic RAM callback table `0x10319D2C`.
2. **Proven Logical Menu Record Dispatch**: Traces verified 16-byte logical records through `COPNOQueueButton` (`0x9013DDCC`), translation `0x90252D9E`, and active screen method dispatch in `CMENUTopMenuView`.

> [!WARNING]
> **UNPROVEN Boundary**: The connection between the GPIO Type-2 Transport and the Logical Menu Record Dispatch is **UNPROVEN**.
> - GPIO callback `0x9011DD5C` / `0x9011E756` creates an outgoing first halfword of `0x0002`.
> - At `COPNOQueueButton` (`0x9013DDCC`), halfword `0x0002` trips the range check (`2 - 4 = 0xFFFFFFFE >= 0x12`) and branches to `0x9013DE14`, completely bypassing the button TBB table (`0x9013DDE4`).
> - Furthermore, type-2 handler `0x9011E2B4` calls `0x9011E7F8(2)` which branches through dynamic RAM table `0x10319D2C` (`+0x18`). Because this table is populated at runtime in uninitialized RAM, no static ROM linkage connects it to logical menu records (`0x9011E08E`, `0x902AE0DA`, `0x90252D9E`).

### Proven Facts (`PROVEN`):
1. **GPIO Pin 8 Debouncer (`0x9011DDA8`)**: Reads GPIO pin 8 via `0x902742C2(pin=8, dir=0, &val)`. On pin state change, calls forwarder `0x9011E738` which invokes `0x90027316` (trigger) with Slot 3 token `0x00000008` loaded from `[0x1011FFB4 + 0x08]`.
2. **Persistent Callback (`0x9011DD5C`)**: Registered in persistent dispatcher Slot 3. Queries pin state (`0x90369364`), stores state in `[0x1011FFB4 + 0x04]`, and tail-calls `0x9011E756`.
3. **Message Buffer Sender (`0x9011E756`)**: Executes `push {r0-r4, lr}`, writes `msg[0] = 0x02` (event type 2 via `strb.w r0, [sp]`), and executes `tk_snd_mbf(mbfid, msg=sp, msgsz=16, tmout=100)` with `mbfid = [0x1011FFB4 + 0x18]`. Bytes 1..15 are the stack image of saved incoming registers (`r0..r3`); semantics **UNPROVEN** (not padding/uninitialized/reserved). Outgoing first halfword is `0x0002`.
4. **Receiver Task Loop (`0x9011DD88`)**: Blocks on `tk_rcv_mbf(mbfid, msg=sp, tmout=-1)`. Validates `msgsz == 16` and `msg[0] < 3`. Dispatches via table `0x9128126C`:
   - Index 0: `0x9011E1F6` (type 0)
   - Index 1: `0x9011E284` (type 1)
   - Index 2: `0x9011E2B4` (type 2: invoked for callback `0x9011DD5C`)
5. **Type 2 Handler (`0x9011E2B4`)**: Evaluates pin transition state, calls `0x9011E7F8(2)` to load handler pointer from RAM table `[0x10319D2C + 0x18 + 0x04]`, and branches via `bx r1` with `r0` = 0 or 1.
6. **ROM Event Translation (`0x9011E08E`)**: Translates raw event indices via Table 1 (`0x91281278`, 16 bytes) and Table 2 (`0x91281287`, 11 bytes) to produce logical button codes `0x04..0x0E`.
7. **16-Byte Queue Record Assembly (`0x902AE122..0x902AE13E`)**: Inside `0x902AE0DA`, assembles `<HHIII` (`event_id`, `tag`, `value_1`, `value_2`, `mask`) and copies 16 bytes via `memcpy` (`0x90274BBC`) into the queue buffer (`0x1012B908`).
8. **COPNOQueueButton Handler (`0x9013DDCC`)**: Vtable `0x9151DE6C` slot `+0x2C`. Subtracts 4 (`subs r1, r1, #4`), range-checks `< 18` (`0x04..0x15`), and filters via TBB at `0x9013DDE4`. Outgoing halfword `0x0002` trips the range check and branches to straight-copy `0x9013DE14`.
9. **Menu Controller Queue Consumer (`0x90032CA4 -> 0x90032B86`)**: Reads 16-byte record from `COPNOHandler` (`0x103C5978` / `0x10128704`). Passes `record.event` to `0x90252D9E`.
10. **Menu Logical ID Translation (`0x90252D9E`)**: Evaluates TBB at `0x90252DA6` with base `0x90252DAA` across 59 entries:
    - Input `0x0C` -> logical ID `0x01`
    - Input `0x0B` -> logical ID `0x1B`
    - Input `0x10` -> logical ID `0x05`
11. **Active Screen Dispatch (`CMENUTopMenuView`)**: Vtable `0x9151D874`:
    - Slot `+0x10` (`0x9025A85E`): Validates logical ID `< 100`, routes IDs `0x01..0x1C` to `0x9025A8D2`, calling slot `+0x40` (`0x9025AD54`).
    - Slot `+0x40` (`0x9025AD54`): Checks `param.value_1 == 1` (`[r6, #0x0C]`), invokes slot `+0x44` (`0x90254DD4`).
    - Slot `+0x44` (`0x90254DD4`): TBB at `0x90254E18` routes:
      - `logical_id = 0x01`: calls slot `+0x12C` (`0x90076FF4`) [inference: Confirm / Item Action]
      - `logical_id = 0x1B`: calls slot `+0x17C` (`0x90077CBD`) [inference: Menu Toggle / Back]
      - `logical_id = 0x05`: calls slot `+0x130` (`0x90077512`) [inference: Nav Right]

### Unproven Boundaries (`UNPROVEN`):
1. **GPIO Pin 8 to Logical Code `0x0B..0x15`**:
   The callback `0x9011DD5C` only enqueues message type 2. Handler `0x9011E2B4` calls the channel 2 handler from RAM table `0x10319D2C + 0x18`. Because table `0x10319D2C` is dynamically registered at runtime in RAM, there is **zero static ROM linkage** tying Pin 8 specifically to code `0x0B`, `0x0C`, or any individual code in `0x0B..0x15`.
2. **Physical Button Names ("MENU", "OK", "INFO")**:
   There are no static ROM strings, RTTI entries, or diagnostic symbols linking GPIO Pin 8 to physical camera legends. Speculating on button names without hardware verification is prohibited.

---

## 2. Decoupled Pipeline Stages

### Graph A: GPIO Type-2 Transport

#### Stage 1: GPIO Pin 8 Debouncer & Forwarder
- **Function**: `0x9011DDA8` (Thumb-2)
- **GPIO Read**: `0x902742C2(r0=8, r1=0, r2=sp)`
- **Forwarder Call-Sites**:
  - `0x9011DDFA`: `bl 0x9011E738` (assert)
  - `0x9011DE0E`: `bl 0x9011E738` (deassert)
- **Forwarder Function (`0x9011E738`)**:
```arm
0x9011e738      push  {r4, lr}
0x9011e73a      mov   r4, r0
0x9011e73c      ldr   r0, [0x9011e8b8]    ; [0x9011e8b8] = 0x1011ffb4
0x9011e73e      ldr   r0, [r0, #8]        ; load token for Slot 3 (0x00000008)
0x9011e740      cbz   r0, 0x9011e746
0x9011e742      bl    0x90027316          ; SignalPersistentDispatcher(mask=0x08)
0x9011e746      cmp   r4, 1
0x9011e748      beq   0x9011e754
```

#### Stage 2: Persistent Callback Slot 3
- **Function**: `0x9011DD5C` (Thumb-2)
- **Token / Mask**: `0x00000008` (Slot 3)
```arm
0x9011dd5c      push  {r4, lr}
0x9011dd5e      blx   0x90369364          ; query hardware pin state (A32 gate)
0x9011dd62      cmp   r0, 1
0x9011dd64      beq   0x9011dd68
0x9011dd66      movs  r0, 0
0x9011dd68      ldr   r1, [0x9011e168]    ; [0x9011e168] = 0x1011ffb4
0x9011dd6a      str   r0, [r1, #4]        ; store state in [0x1011ffb4 + 4]
0x9011dd6c      pop.w {r4, lr}
0x9011dd70      b.w   0x9011e756          ; tail-call sender helper
```

#### Stage 3: Message Buffer Sender
- **Function**: `0x9011E756` (Thumb-2)
- **Operation**: `push {r0-r4, lr}` saves incoming registers. `strb.w r0, [sp]` writes only byte 0 with `0x02`.
- **Payload Layout**: Bytes 1..15 are the stack image of saved incoming registers (`r0..r3`); semantics **UNPROVEN** (not padding/uninitialized/reserved).
- **Outgoing Halfword**: `0x0002`.
```arm
0x9011e756      push  {r0, r1, r2, r3, r4, lr}
0x9011e758      movs  r0, 2               ; event type 2
0x9011e75a      movs  r3, 0x64            ; timeout = 100 ms
0x9011e75c      movs  r2, 0x10            ; msgsz = 16 bytes
0x9011e75e      strb.w r0, [sp]           ; msg[0] = 0x02
0x9011e762      mov   r1, sp              ; buffer = sp
0x9011e764      ldr   r0, [0x9011e8b8]    ; [0x9011e8b8] = 0x1011ffb4
0x9011e766      ldr   r0, [r0, #0x18]     ; mbfid = [0x1011ffb4 + 0x18]
0x9011e768      bl    0x9002ec0e          ; tk_snd_mbf(mbfid, msg, msgsz=16, tmout=100)
0x9011e76c      pop   {r0, r1, r2, r3, r4, pc}
```

#### Stage 4: Receiver Task Loop
- **Function**: `0x9011DD88` (Thumb-2)
- **Task Syscall**: `tk_rcv_mbf(mbfid, msg=sp, tmout=-1)` via stub `0x90225E54`
```arm
0x9011dd88      mov   r1, sp              ; 16-byte receive buffer
0x9011dd8a      ldr   r0, [r4, #0x18]     ; mbfid = [0x1011ffb4 + 0x18]
0x9011dd8c      mov   r2, r5              ; tmout = -1 (TMO_FEVR)
0x9011dd8e      blx   0x90225e54          ; tk_rcv_mbf
0x9011dd92      cmp   r0, 0x10            ; assert received size == 16 bytes
0x9011dd94      bne   0x9011dd88
0x9011dd96      ldrb.w r0, [sp]           ; read msg[0] (event type)
0x9011dd9a      cmp   r0, 3               ; valid types: 0, 1, 2
0x9011dd9c      bge   0x9011dd88
0x9011dd9e      ldr.w r1, [r7, r0, lsl #2]; r7 = 0x9128126c (dispatch table)
0x9011dda2      adds  r0, r6, 1           ; r0 = sp + 1 (payload pointer)
0x9011dda4      blx   r1                  ; invoke type handler
0x9011dda6      b     0x9011dd88
```
- **Table `0x9128126C`**:
  - `[0] = 0x9011E1F7`: Type 0 handler (`0x9011E1F6`)
  - `[1] = 0x9011E285`: Type 1 handler (`0x9011E284`)
  - `[2] = 0x9011E2B5`: Type 2 handler (`0x9011E2B4`)

#### Stage 5: Type 2 Handler & Dynamic RAM Boundary
- **Function**: `0x9011E2B4` (Thumb-2)
```arm
0x9011e2b4      push  {r4, r5, r6, lr}
0x9011e2b6      ldr   r4, [0x9011e620]    ; [0x1011ffb4]
0x9011e2b8      ldr   r0, [r4, #4]        ; read pin state
0x9011e2ba      cmp   r0, 2
0x9011e2bc      beq   0x9011e318
...
0x9011e2de      movs  r0, 2               ; channel index 2
0x9011e2e0      bl    0x9011e7f8          ; lookup callback pointer in RAM table
0x9011e2e4      mov   r1, r0              ; r1 = callback func ptr
...
0x9011e30e      and   r0, r0, 1           ; r0 = state (0 or 1)
0x9011e312      pop.w {r4, r5, r6, lr}
0x9011e316      bx    r1                  ; dispatch to registered channel handler!
```
- **Table Indexer (`0x9011E7F8`)**:
```arm
0x9011e7f8      ldr   r1, [0x9011e8c0]    ; [0x9011e8c0] = 0x10319d2c (RAM table!)
0x9011e7fa      add.w r0, r0, r0, lsl #1  ; r0 = r0 * 3
0x9011e7fe      add.w r0, r1, r0, lsl #2  ; r0 = 0x10319d2c + r0 * 12
0x9011e802      ldr   r0, [r0, #4]        ; r0 = *(entry + 4)
0x9011e804      bx    lr
```
The dispatch terminates at `bx r1` where `r1` is read from uninitialized RAM table `0x10319D2C`. This represents a dynamic boundary.

---

### Graph B: Proven Logical Menu Record Dispatch

#### Stage 6: Event Code Translation in ROM
- **Function**: `0x9011E08E` (Thumb-2)
- **Table 1 (`0x91281278`, 16 bytes)**:
  `[11, 11, 11, 11, 0, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 4]`
- **Table 2 (`0x91281287`, 11 bytes)**:
  `[4, 5, 6, 9, 7, 10, 8, 11, 12, 13, 14]`
- Output codes: `0x04, 0x05, 0x06, 0x09, 0x07, 0x0A, 0x08, 0x0B, 0x0C, 0x0D, 0x0E`.

#### Stage 7: 16-Byte Queue Record Assembly
- **Location**: `0x902AE122..0x902AE13E` inside `0x902AE0DA` (`COPNOQueue::EnqueueRecord`)
- **Structure Format**: `<HHIII` (16 bytes)
```arm
0x902ae122      strh.w r6, [sp]           ; +0x00: event_id (u16)
0x902ae126      ldrh   r0, [r5, #2]       ; +0x02: tag (u16)
0x902ae128      movs   r2, 0x10           ; size = 16 bytes
0x902ae12a      mov    r1, sp             ; src = stack buffer
0x902ae12c      strh.w r0, [sp, #2]
0x902ae130      ldr    r0, [r5, #4]       ; +0x04: value_1 (u32)
0x902ae132      str    r0, [sp, #4]
0x902ae134      ldr    r0, [r5, #8]       ; +0x08: value_2 (u32)
0x902ae136      str    r0, [sp, #8]
0x902ae138      ldr    r0, [r4, #0x10]    ; +0x0C: queue mask (u32)
0x902ae13a      str    r0, [sp, #0xc]
0x902ae13c      mov    r0, r7             ; dest queue buffer
0x902ae13e      blx    0x90274bbc         ; memcpy(dest=r7, src=sp, 16)
```

#### Stage 8: COPNOQueueButton Filter
- **Address**: `0x9013DDCC` (Thumb-2), vtable `0x9151DE6C` slot `+0x2C`
- **Range Check**:
```arm
0x9013ddd2      ldrh  r1, [r1]            ; read event_id from record
0x9013dddc      ldr   r0, [r5, #4]        ; read value_1
0x9013ddde      subs  r1, r1, 4           ; r1 = event_id - 4
0x9013dde0      cmp   r1, 0x12            ; range < 18 (0x04..0x15)
0x9013dde2      bhs   0x9013de14          ; out-of-range: straight copy (bypasses TBB)
0x9013dde4      tbb   [pc, r1]            ; branch table at 0x9013dde8
```

#### Stage 9: Menu Controller Queue Read & Logical ID Translation
- **Function**: `0x90032B86` (Thumb-2), called from slot `+0x64` (`0x90032CA4`)
- Reads 16-byte record from `COPNOHandler` singleton (`0x103C5978` / `0x10128704`)
- Passes `record.event` to `0x90252D9E` (`TranslateMenuLogicalEventID`):
```arm
0x90252d9e      cmp   r1, 0x3b            ; bounds check < 59
0x90252da0      mov.w r0, -1              ; default unmapped
0x90252da4      bhs   0x90252e56
0x90252da6      tbb   [pc, r1]            ; table at 0x90252daa
```

##### Complete TBB Translation Table (`0x90252DAA`, 59 entries):
| Input ID | Target | Logical ID | Return Code | Inferred Role |
|---|---|---|---|---|
| `0x0B` | `0x90252E40` | `0x1B` | 0 | Menu Toggle / Back (INFERENCE) |
| `0x0C` | `0x90252DE6` | `0x01` | 0 | Menu Confirm / Select Action (INFERENCE) |
| `0x0D` | `0x90252DEA` | `0x02` | 0 | Navigation Slot 2 |
| `0x0E` | `0x90252DF0` | `0x03` | 0 | Navigation Slot 3 |
| `0x0F` | `0x90252DF4` | `0x04` | 0 | Navigation Slot 4 |
| `0x10` | `0x90252DF8` | `0x05` | 0 | Navigation Slot 5 / Nav Right (INFERENCE) |
| `0x11` | `0x90252DFC` | `0x06` | 0 | Navigation Slot 6 / Nav Left (INFERENCE) |
| `0x12` | `0x90252E00` | `0x07` | 0 | Navigation Slot 7 / Nav Up (INFERENCE) |
| `0x13` | `0x90252E04` | `0x08` | 0 | Navigation Slot 8 / Nav Down (INFERENCE) |
| `0x14` | `0x90252E08` | `0x09` | 0 | Navigation Slot 9 |
| `0x15` | `0x90252E0C` | `0x0A` | 0 | Navigation Slot 10 |
| `0x16` | `0x90252E10` | `0x0B` | 0 | Navigation Slot 11 |
| `0x17` | `0x90252E14` | `0x0C` | 0 | Navigation Slot 12 |
| `0x18` | `0x90252E3C` | `0x1A` | 0 | Navigation Slot 26 |
| `0x19` | `0x90252E38` | `0x19` | 0 | Navigation Slot 25 |
| `0x1A` | `0x90252E44` | `0x1C` | 0 | Navigation Slot 28 |
| `0x28` | `0x90252E48` | `0x0E` | 0 | Navigation Slot 14 |
| `0x2B` | `0x90252E50` | `0x16` | 1 | Special Gate |
| `0x3A` | `0x90252E58` | `0x18` | 0 | Navigation Slot 24 |

#### Stage 10: Param Creation & Active Screen Dispatch
- On return 0 from `0x90252D9E`:
  - `0x90032C18`: calls `0x9012B82A` to instantiate `CJAPFButtonEventParam` (vtable `0x9152BD0C`, RTTI `21CJAPFButtonEventParam`).
  - `0x90032C3A`: sets `param + 0x0C = record.value_1`.
  - `0x90032C7C`: retrieves active screen from controller list (`0x90031D2A -> 0x9003FF50`).
  - Calls active screen slot `+0x10` (`0x9025A85E` for `CMENUTopMenuView`) with `r1 = logical_id`, `r2 = param_ptr`.
- **In `CMENUTopMenuView::Slot10` (`0x9025A85E`)**:
  - Logical IDs `0x01..0x1C` route to `0x9025A8D2`, which tail-calls slot `+0x40` (`0x9025AD54`).
  - In `0x9025AD54`, if `param.value_1 != 0`, calls slot `+0x54` and then slot `+0x44` (`0x90254DD4`).
  - In `0x90254DD4`, TBB at `0x90254E18` routes:
    - `logical_id = 0x01` (from input `0x0C`): calls slot `+0x12C` (`0x90076FF4`) [inference: Confirm / Item Action]
    - `logical_id = 0x1B` (from input `0x0B`): calls slot `+0x17C` (`0x90077CBD`) [inference: Menu Toggle / Back]
    - `logical_id = 0x05` (from input `0x10`): calls slot `+0x130` (`0x90077512`) [inference: Nav Right]

---

## 3. Proven vs Unproven Summary Table

| Pipeline Entity | ROM Address / Ref | Status | Evidence |
|---|---|---|---|
| GPIO Pin 8 read | `0x9011DDA8` | **PROVEN** | `movs r0, #8; bl 0x902742C2` |
| Slot 3 forwarder | `0x9011E738` | **PROVEN** | `ldr r0, [0x1011FFB4+8]; bl 0x90027316` |
| Persistent callback | `0x9011DD5C` | **PROVEN** | `blx 0x90369364; str r0, [r1, 4]; b 0x9011E756` |
| Message sender | `0x9011E756` | **PROVEN** | `push {r0-r4,lr}; strb byte0=2; bl 0x9002EC0E` |
| Receiver loop | `0x9011DD88` | **PROVEN** | `blx 0x90225E54; cmp r0, 0x10; ldr r1, [0x9128126C, r0, lsl 2]` |
| Type 2 handler | `0x9011E2B4` | **PROVEN** | `bl 0x9011E7F8(2); bx r1` |
| Record assembler | `0x902AE122` | **PROVEN** | `strh r6, [sp]; strh r0, [sp, 2]; str r0, [sp, 4]; str r0, [sp, 8]; str r0, [sp, 0xc]; memcpy` |
| COPNOQueueButton | `0x9013DDCC` | **PROVEN** | Vtable `0x9151DE6C` slot `+0x2C`, TBB `0x9013DDE4` |
| Menu Logical Translator | `0x90252D9E` | **PROVEN** | TBB `0x90252DAA`, maps `0x0C -> 1`, `0x0B -> 0x1B`, `0x10 -> 5` |
| Menu Slot +0x12C | `0x9151D874 + 0x12C`| **PROVEN** | `0x90076FF4` (inferred role: Confirm / Item Action) |
| Menu Slot +0x17C | `0x9151D874 + 0x17C`| **PROVEN** | `0x90077CBD` (inferred role: Menu Toggle / Back) |
| Menu Slot +0x130 | `0x9151D874 + 0x130`| **PROVEN** | `0x90077512` (inferred role: Nav Right) |
| Pin 8 -> Logical `0x0B/0x0C` | Dynamic RAM | **UNPROVEN** | Table `0x10319D2C` is dynamically linked in RAM; halfword 0x0002 bypasses button TBB |
| Pin 8 -> "MENU/OK" legends | N/A | **UNPROVEN** | Zero static ROM strings or symbols associate physical Pin 8 with button legends |
