# Cross-Architecture Analysis: Nikon EXPEED 4 (D3300) vs. EXPEED 6 (Z 6II / qemu-expeed)

## 1. Context & Significance

In November 2026, software engineer Arne Baeyens published **qemu-expeed** and a working native port of **DOOM** on the Nikon Z 6II (dual EXPEED 6 SoC), accompanied by an article on NikonRumors.

This report analyzes the architecture, execution vectors, and reverse-engineering findings of `qemu-expeed` and evaluates how they map directly to our **D3300 C 1.02 (EXPEED 4)** research.

---

## 2. Core Architectural Parallels

| Feature | Nikon Z 6II (EXPEED 6) | Nikon D3300 (EXPEED 4) |
|---|---|---|
| **SoC Lineage** | Socionext Milbeaut (dual Cortex-A7) | Socionext Milbeaut (Cortex-A9) |
| **Operating System** | **T-Kernel** (μITRON specification) | **T-Kernel 2.0** (identified at `0x9154EE20`) |
| **Syscall Mechanism** | `svc #6` with code in `IP` (e.g. `0x80180100`) | RAM dispatch table `0x10000000` / SWI thunks |
| **Task Creation** | `tk_cre_tsk` (`0x101044EC`), `tk_sta_tsk` | `0x10000028` (`tk_cre_tsk`), `0x90226040` thunk |
| **Hardware Reference** | GoPro GP1 / GP2 GPL kernel patches | Socionext Milbeaut M-series reference drivers |
| **Non-Volatile RAM** | DRAM above OS space (`0xD0000000`) | RAM space (`0x10000000..0x104A0000`) |
| **Display Overlay** | Hardware planes (`Z6II_DISPENG + 0x6440`) | DrawBuffer 20-slot descriptor table (`0x104A0868`) |
| **PTP Service Mode** | `0x94FE` / `0x94FF` with sub-ops `0xF036`/`0xF037` | Vendor PTP commands in table `0x9151F000..0x91540000` |

---

## 3. The Non-Destructive Payload Execution Pattern

Arne Baeyens' DOOM loader (`scripts/usb/load_doom.py`) demonstrates how to execute custom native code on a retail Nikon camera **without flashing SPI ROM or risking bricking**.

### 3.1 PTP Service Wrap Protocol
Nikon's PTP implementation includes proprietary vendor extensions:
- `OP_SERVICE_WRAP_RECV (0x94FF)`
- `OP_SERVICE_WRAP_SEND (0x94FE)`

Sub-commands provide raw memory access:
- `SUBOP_F001 (0xF001)`: Unlock / enable service access (`0x2001` OK).
- `SUBOP_F036 (0xF036)`: Read physical memory buffer (`addr`, `length`).
- `SUBOP_F037 (0xF037)`: Write physical memory buffer (`addr`, `data`).

### 3.2 SGI (Software Generated Interrupt) Hijacking
To execute uploaded code without hooking ROM:
1. **Upload**: Binary payload is written to high DRAM (`0xD0000000`).
2. **Hook**: The second-level ISR table (`IRQTAB_BASE + 4 * sgi`) is read and temporarily overwritten with `BLOB_ADDR | 1` (Thumb entry point).
3. **Trigger**: Writing `0x02000000 | sgi` to ARM GIC distributor register `GICD_SGIR` (`0x04301F00`) triggers an immediate interrupt to the core itself.
4. **Spawn**: The interrupt handler allocates a `T_CTSK` structure, calls `tk_cre_tsk` and `tk_sta_tsk` to create a dedicated background T-Kernel task, restores the original ISR entry in `IRQTAB`, and returns from the interrupt.
5. **Execution**: The spawned task runs freely with access to camera services.
6. **Cleanup**: On exit, calling `tk_ext_tsk()` terminates the task. On power-off or battery removal, DRAM clears completely with zero persistent modifications.

---

## 4. Task Quieting (`tk_sus_tsk`)

A critical challenge when running custom UI overlays or heavy computational routines (e.g., focus peaking, intervalometer, raw histograms) is avoiding contention with the camera's built-in UI and imaging tasks.

Baeyens' approach in `z6ii_quiet()`:
- Traverses the T-Kernel TCB (Task Control Block) table (`Z6II_KNL_TCB_TABLE` with stride `Z6II_TCB_STRIDE`).
- Inspects task state (`tcb[0x28/4]`) and context (`exinf`).
- Calls `tk_sus_tsk()` on UI and imaging tasks while leaving kernel and audio streaming tasks active.
- On exit, `z6ii_unquiet()` calls `tk_rsm_tsk()` to seamlessly hand control back to the stock Nikon firmware.

---

## 5. Socionext Milbeaut & GoPro Clues

A major revelation from `qemu-expeed` is that Nikon EXPEED SoCs are built on **Socionext Milbeaut** IP, sharing identical peripheral registers with GoPro's GP1 and GP2 processors:
- **HDMAC**: High-speed DMA controller.
- **MIPU**: Multi-stage Image Processing Unit (Bayer raw development pipeline).
- **SFLT / RAWB / DAFI / MVD**: Optical defect correction, noise reduction, and video hardware encoders.
- **IIP**: Image Information Processor.
- **EXMCU**: External microcontroller bridge for power switch, dial encoders, and button multiplexing.

Because GoPro released open-source Linux kernel GPL patches for GP1/GP2, these driver trees serve as an open register-level reference for Nikon EXPEED peripheral modeling and reverse engineering.

---

## 6. Actionable Takeaways for D3300-ML

1. **Architecture Confirmed**: The D3300-ML module model (`core/dml_tkernel.c`, `payload/dml_payload.c`) perfectly matches the reality of EXPEED execution: standalone relocatable modules executing as native T-Kernel tasks.
2. **Safe Host Emulation (QEMU)**: The stall-breaker approach (returning all-ones for unmodeled MMIO registers) combined with Cortex-A9 CPU core emulation allows testing firmware execution loops in QEMU before testing on hardware.
3. **Zero-Flash Delivery**: Firmware extensions can be tested non-destructively via memory injection without touching flash storage.
