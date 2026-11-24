# Nikon D3300 C 1.02 — Verified Menu Architecture & Dispatch Chain

Static analysis of Nikon D3300 Firmware C 1.02 (`b860_102.bin`, SHA-256: `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`).

---

## 1. Class Hierarchy & RTTI Descriptors

The Nikon GUI architecture uses standard GCC Itanium C++ ABI RTTI structures located in ROM (`0x90020000..0x91600000`):

```text
[CUIFAViewBase] (RTTI 0x915015E8, __class_type_info)
   ▲
   │
[CJAPFView] (RTTI 0x9150866C, __si_class_type_info)
   ▲
   │
[CMENUViewBase] (RTTI 0x915012CC, __vmi_class_type_info)
   ▲  ├── inherits IGUICDisableStatusAccessorIF (RTTI 0x91507AF4)
   │
[CMENUTopMenuView] (RTTI 0x91502354, __vmi_class_type_info)
      ├── inherits IGUICSettingValueAccessorIF (RTTI 0x91507758)
      └── vtable: 0x9151D874
```

Controller hierarchy:
```text
[CJAPFController] (RTTI 0x91501C90, __si_class_type_info)
   ▲
   │
[CMENUController] (RTTI 0x91501D8C, __si_class_type_info)
      └── vtable: 0x9151B62C
```

Entry container:
```text
[CGUICEntry] (RTTI 0x91500AC8, vtable 0x91515E84)
   ├── offset +0x00: vtable 0x91515E84
   ├── offset +0x08: active view pointer (CGUICView* / CMENUTopMenuView*)
   └── offset +0x0C: screen ID
```

---

## 2. Event Pump & Dispatch Call Chain

The verified execution path from queue retrieval to virtual method execution:

```text
1. Queue Consumer Loop: CMENUController::ProcessEventQueue (slot +0x64 at 0x90032CA4)
   │
   ▼
2. Record Reader: 0x90032B86
   │ Reads 16-byte logical record (<HHIII) from COPNOHandler queue
   │ If event == 0xFFFF -> flushes queue
   │
   ▼
3. Logical Event Translation: 0x90252D9E
   │ TBB at 0x90252DA6 (base 0x90252DAA, 59 entries)
   │ Maps raw input codes:
   │   0x0C -> 0x01 [inferred role: Confirm / Item Action]
   │   0x0B -> 0x1B [inferred role: Menu Toggle / Back]
   │   0x10 -> 0x05 [inferred role: Navigation Right]
   │
   ▼
4. Param Object Construction: 0x9012B82A
   │ Instantiates CJAPFButtonEventParam (RTTI 0x9152BD0C)
   │ Populates param + 0x0C = record.value_1
   │
   ▼
5. Screen Lookup & Router: 0x90032AFA (called by CMENUController slot +0x60)
   │ Calls 0x90031D2A -> 0x9003FF50
   │
   ▼
6. Active Screen Getter: 0x9003FF50
   │ Loads container from [controller + 0x18] + 0x0C
   │ Calls 0x9011B860 (size check > 0)
   │ Calls 0x9011B838(list, 0, &entry) (fetch index 0)
   │ Extracts active view pointer: r0 = *(uint32_t*)(entry + 8)
   │
   ▼
7. Virtual Method Invocation: 0x90032B0E
   │ Loads vtable from active view: r1 = *active_view
   │ Loads slot +0x10: r3 = *(vtable + 0x10)
   │ Calls: blx r3 with (r0=active_view, r1=logical_id, r2=param)
   │
   ▼
8. Screen Entry Handler: CMENUTopMenuView slot +0x10 (0x9025A85E)
   │ TBB at 0x9025A86E (100 entries)
   │ Codes 1..28 (0x01..0x1C) branch to 0x9025A8D2
   │ Verifies param != NULL, calls slot +0xC4 pre-handler
   │ Tail-calls slot +0x40 (0x9025AD54)
   │
   ▼
9. Screen Event Router: CMENUTopMenuView slot +0x40 (0x9025AD54)
   │ Calls slot +0x54 (pre-action hook)
   │ Calls slot +0x44 (0x90254DD4) with (this, logical_id, param)
   │
   ▼
10. Action Method Router: CMENUTopMenuView slot +0x44 (0x90254DD4)
    │ TBB at 0x90254E18 (base 0x90254E1C, 29 entries)
    │ Dispatches to dedicated virtual methods:
    │   Code  1 (0x01): Slot +0x12C = 0x90076FF4 [inferred role: Confirm / Item Action]
    │   Code  2 (0x02): Slot +0x128 = 0x90076E70 (Action 2)
    │   Code  5 (0x05): Slot +0x130 = 0x90077512 [inferred role: Nav Right]
    │   Code  6 (0x06): Slot +0x134 = 0x90077540 [inferred role: Nav Left]
    │   Code  7 (0x07): Slot +0x138 = 0x90077548 [inferred role: Nav Up]
    │   Code  8 (0x08): Slot +0x13C = 0x90077550 [inferred role: Nav Down]
    │   Code  9 (0x09): Slot +0x140 = 0x900776EC (Action 9)
    │   Code 10 (0x0A): Slot +0x144 = 0x900776EE (Action 10)
    │   Code 27 (0x1B): Slot +0x17C = 0x90077CBD [inferred role: Menu Toggle / Back]
    │   Code 28 (0x1C): Slot +0x180 = 0x90255AE6 (Action 28)
```

---

## 3. Passive Observation Seams

For non-destructive extension modules that need to track menu activity without modifying camera code or state:

1. **Active Screen Observation Seam**:
   - Location: RAM pointer at `0x10123844`.
   - Populated by `CMENUController` at `0x9002A6AC` (`str r0, [0x10123844]`).
   - Content: pointer to currently active `CGUICView` / `CMENUTopMenuView`.
   - Safety: **Read-Only**. Reading `0x10123844` introduces zero side effects.

2. **Screen Stack Inspection**:
   - Location: `[controller + 0x18] + 0x0C` (container list).
   - Element 0 points to active `CGUICEntry`.
   - `*(uint32_t*)(entry + 8)` gives the active view pointer.
   - `*(uint32_t*)(entry + 0x0C)` gives the active screen ID.
   - Safety: **Read-Only**.

3. **Button Event Tap**:
   - Mailbox getter `0x9035128C` safely reads the last 16-byte button message from `[state + 0x30]`.
   - Returns ready flag (`state + 0x10`).
   - Callers: `0x903292C6`, `0x9032936A`, `0x90332E58`, `0x90332FAC`.
   - Safety: **Read-Only**.

---

## 4. Conceptual Extension Architecture

An extension framework for custom menus (such as intervalometer, film simulation, photo assist) can be structured with clean separation of concerns:

```text
┌────────────────────────────────────────────────────────┐
│                   Module Manager                       │
│  (Orchestrates lifecycle: init, update, render, exit)  │
└───────┬───────────────────┬────────────────────┬───────┘
        │                   │                    │
        ▼                   ▼                    ▼
┌──────────────┐    ┌──────────────┐    ┌─────────────────┐
│ EventAdapter │    │  MenuModel   │    │ RendererAdapter │
│ (Read-Only   │    │ (State Tree, │    │ (OSD Primitives │
│  Event Tap)  │    │  Navigation) │    │  0x903111BC)    │
└──────────────┘    └──────────────┘    └─────────────────┘
```

1. **EventAdapter**:
   - Taps logical IDs: `0x01` (Slot +0x12C), `0x05..0x08` (Slots +0x130..+0x13C), `0x1B` (Slot +0x17C).
   - While custom menu is inactive, all events pass through unaffected.
   - When custom overlay is engaged, events update the custom `MenuModel`.

2. **MenuModel**:
   - Represents custom menu hierarchy: Categories -> Items -> Values/Actions.
   - Tracks cursor position, active tab, edit mode, and parameter ranges.

3. **RendererAdapter**:
   - Emits 10-byte vector primitives (`0x903111BC`) or direct frame-buffer overlays.
   - Uses `COSDDControl` / `COSDCRectInfo` bounding boxes to avoid corrupting native display planes.

4. **ModuleManager**:
   - Hosts decoupled modules (`Intervalometer`, `FujiFilmSim`, `PhotoAssist`).
   - Each module implements uniform hooks: `init()`, `handle_key(key)`, `render(ctx)`, `destroy()`.
