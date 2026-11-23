# Milestone Target: D3300-ML Custom Information Display Screen

## 1. Objective

The primary user-facing end goal of the **D3300-ML** project is to replace Nikon's dated factory shooting information display ("info screen" showing Shutter Speed, Aperture, ISO, Battery, Exposure Compensation, Picture Control, AF mode, and remaining frame count) with a custom, sleek, modern, and readable D3300-ML information screen.

---

## 2. Technical Building Blocks & Architecture

The custom screen will be rendered dynamically onto the camera's LCD without modifying camera ROM:

```
+-------------------------------------------------------------------+
|                     D3300-ML Custom Info Screen                   |
+-------------------------------------------------------------------+
|  Camera Status State Machine (`CKRRCCameraStatus` / `CINFO` API)  |
|  - Shutter speed, Aperture (f-number), ISO, Metering, Battery     |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                T-Kernel Quieting Engine (`dml_tkernel`)           |
|  - `dml_tk_quiet_tasks()`: suspends factory info screen worker    |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|             ARGB8888 Rasterizer (`core/dml_draw_buffer.c`)        |
|  - 720x480 resolution at RAM `0x104A0868`                         |
|  - Modern typography, high-contrast exposure readout, battery bar |
|  - Vsync buffer swap via `dml_draw_buffer_swap(table, idx, 1)`    |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|              Interactive Button Dispatcher (`dml_payload.c`)      |
|  - Toggle screen via 'info' button or MENU button                 |
|  - Dial rotation adjusts Shutter / Aperture / ISO                 |
+-------------------------------------------------------------------+
```

---

## 3. Data Sources from Reverse-Engineered Seams

1. **Camera Shooting Status (`17CKRRCCameraStatus` at RAM `0x10491580`)**:
   - Vtable `0x9151F6F0`, installed by `0x9013BC60` / `0x9019A340`.
   - Method `+0x14` (`0x90350DD0`) reads field `+0x84`, providing current shutter speed, aperture, and exposure lock status.
2. **Shooting Menu Parameters (`27CINFOModifyDataShootingMenu` at `0x915073A4`)**:
   - Vtable at `0x9154050C`.
   - Provides active Picture Control, White Balance, and RAW/JPEG quality.
3. **Battery & Power State (`CSYSCPowerManager` / `TYSensReadPort` at `0x912838A0`)**:
   - Provides real-time battery voltage and percentage.
4. **Framebuffer Target (`DrawBuffer` at RAM `0x104A0868`)**:
   - 720×480 32 bpp ARGB8888 canvas with 20 slots.
   - Provides smooth, tear-free rendering using hardware Vsync synchronization (`DML_DRAW_BUFFER_QUERY_SYNC = 0x3110`).

---

## 4. Implementation Phasing

1. **Phase 1: Mock Layout in Host Simulator (`sim/main.c`)**:
   - Design the modern info screen layout (clean layout: large shutter speed, aperture f-number, ISO values, EV scale, battery meter, shots remaining).
   - Test rendering and export reference screenshot using `--screenshot info_screen.bmp`.
2. **Phase 2: Status Provider Interface (`core/dml_camera_status.h`)**:
   - Define abstract `DmlCameraStatus` struct (shutter speed fraction, aperture 1/100, ISO, battery %, exposure mode M/A/S/P).
   - Bind mock status provider in simulator and real camera reader in payload.
3. **Phase 3: Hardware Hook Activation**:
   - Wire info button event code (`0x0B` / `0x0C` / info button) to toggle custom info screen on real camera.
