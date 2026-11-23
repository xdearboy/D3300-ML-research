# D3300-ML handoff — 2026-11-23

## Scope and safety

Read-only reverse engineering of Nikon D3300 C 1.02 B-firmware. Do not create an installable firmware image and do not perform camera operations. Do not publish Nikon binaries or analysis ELF files. Every address below was checked against decoded `b860_102.bin` and `d3300_analysis.elf` unless marked unresolved.

## Repository state

- Local repository: `/Users/devo/Documents/antigravity/mysterious-mendeleev`
- GitHub: `https://github.com/xdearboy/D3300-ML-research`
- Remote branch and tags pushed through commit `474e28b`.
- No `.bin`, `.elf`, `.exe`, or firmware ZIP is tracked in Git history.
- Last committed/pushed commit: `474e28b Bound COPNOQueueButton constructor candidates`.

## Verified boundaries, prioritized for integration:

1. **RelayEventEx registration API — best candidate for future event adapter without patching worker:**
   - Register: `0x900272A6`; unregister/cancel by returned token: `0x90027316`.
   - Table contains 32 entries of 8 bytes: `(mask/handle, callback)`. Registration takes callback in `R0`, finds an empty callback slot, stores it, and returns the first word `(1 << slot)`; returns 0 on overflow.
   - Direct lifecycle confirmation: `0x900275D6` passes Thumb callback `0x9002758B`, `0x900275E6` stores return value; numerous calls to `0x90027316` pass previously stored tokens.
   - Worker `0x900271A4`: in `0x900271C8..0x900271DA` tests mask with received event mask and executes `BLX R0`. Callback is NOT cleared before invocation; path is persistent.
   - Limitation: existing callback `0x9002758A` does not read input registers, so exact signature is unproven. Task worker context, but reentrancy, allowed duration, and shutdown order are unresolved.
   - `hook_safety`: unverified, but using registration/lifecycle API is architecturally superior to patching `0x900271DA`.

2. **Regular RelayEvent — suitable for one-shot notifications:**
   - Register: `0x900270F8`, unregister: `0x90027168`, worker: `0x90026FF0`, invocation: `0x9002702C`.
   - Direct lifecycle: `0x90031720` loads callback `0x90031601`, `0x90031722` registers, return token stored in object+`0x0C`; `0x90031498` reads token and `0x9003149A` unregisters.
   - Worker zeroes the callback slot before `BLX R0` (`0x9002702A`): confirmed one-shot path.

3. **Accepted application-button record handoff — best passive observation seam for button stream:**
   - `0x90327628` receives 16-byte record via `0x9024144C → 0x90323D7C`.
   - In `0x903276B2` `R1=SP`; `R0` is destination state object from `[controller+0x24]`; calls `0x903276B4 → 0x903512A0`.
   - `0x903512A0` sets `dest+0x10 = 1` and copies 4 words (16 bytes) to `dest+0x30..+0x3C`.
   - Exact ABI handoff: `R0` = destination state object, `R1` = pointer to 16-byte record.
   - Filters exist before handoff on `record+2` (halfword) and `record+4` (word).

4. **Read-side button seam — verified helper `0x9035128C`:**
   - Candidate ABI: `R0` = state object, `R1` = `NULL` for polling or destination pointer for copying 16-byte record.
   - Reads `ready` flag at `[R0, #0x10]`. If set and `R1 != NULL`, copies 16 bytes from `[R0, #0x30]` via `LDM/STM`. Returns `ready` flag in `R0`.

5. **CMENUController message boundary — candidate for custom menu activation:**
   - RTTI/vtable: `0x91501D8C` / `0x9151B628`.
   - Slot at `0x9151B63C` leads to `0x90031E30`: `R0`=controller, `R1`=message; dispatcher from `controller+0x28` called at `0x9003FD72`.
   - Verified request `0x00020101`: `CUIFAJAppHandle` method `0x9002D546` builds it, `0x90031938`/`0x90031968` send via port, dispatcher routes to `0x900F466C` → controller slot `+0x38` (`0x9002A400`).

6. **CGUICView/CGUICTask boundary — candidate for UI backend:**
   - `CMENUTopMenuView` vtable `0x9151D874`, constructor set at `0x900760FE`, setup `0x900766C8 → 0x90255E56`.
   - `CGUICView` commands: `0x00063402` (method `0x900EEAE6`, send `0x900EEBE0`), `0x00063416` (method `0x900EEC40`, send `0x900EED92`).
   - `CGUICTask` dispatches and calls view slots `+0x10/+0x14` via confirmed branches `0x900EE48C/0x900EE4A4`.

7. **DrawBuffer API — graphics layer:**
   - Functions: create `0x9029E720`, select `0x9029E848`, destroy `0x9029E8B0`, swap `0x9029E940`, query `0x9029E9B2`.
   - Table of 20 entries at `0x104A0868`.

## Negative results and boundaries to avoid

- **Do NOT hook `0x90323E24`**: verified gateway for device-state selectors `0x33/0x35`, not button producer for `0x0B/0x0C`.
- **Do NOT hook `0x9013CF56`**: generic button ID normalizer `0x40..0x4D`; physical MENU/OK unmapped.
- **Do NOT patch worker invocation sites `0x9002702C`, `0x900271DA`**: register/unregister entry points are cleaner and less invasive.
- **CKRCC index 0x0C (`0x10491580`) is CLOSED**: Verified as `17CKRRCCameraStatus` reading path, NOT a button producer.
  - `0x9013BC60` calls constructor `0x9019A340` with `R0 = 0x10491580`.
  - Installs vtable `0x9151F6F0`, RTTI points to `17CKRRCCameraStatus` (`0x9150AFBB`).
  - Slot `+0x14` (`0x90350DD0`) reads field `+0x84`, returns word at `+0x0C` of that structure.
  - Called by `0x900FAB14` for `memcpy` via `0x90274BBC`.
  - Saved `COPNOHandler` alias at `0x1049157C` is an adjacent separate global; adjacency is not evidence of relationship.

## Recommended future architecture

```
RelayEventEx adapter
  → 16-byte event translator at AppButton handoff
  → CMENU message adapter
  → CGUICView backend
  → DrawBuffer backend
```

## Tools added in working tree

- `tools/d3300_derived_refs.py`: tracks short sequences starting from PC-relative RAM literals through MOV/ADD/SUB and reports derived LDR/STR accesses (e.g. `0x1049157C` accessed via `0x10491468 + 0xB4 + 0x60`).
- `tests/test_derived_refs.py`: unit tests for derived reference tracking.
