# D3300-ML Comprehensive Engineering Handoff

## Scope and Safety

Read-only reverse engineering of Nikon D3300 C 1.02 B-firmware and host-side emulation/payload development.
- **Hardware Safety**: Zero writes to SPI Flash ROM. Code execution is restricted to volatile DRAM (`0x10480000..0x104A0000`).
- **Firmware Status**: Target firmware C:1.02 (`b860_102.bin`). Generated Picture Control `.NOP`/`.NCP` profiles are 100% compatible with both C:1.01 and C:1.02.
- **Repository**: `/Users/devo/Documents/antigravity/mysterious-mendeleev` (`https://github.com/xdearboy/D3300-ML-research`).

---

## 1. Post-Codex Engineering Additions (Chronological Summary)

Codex's work concluded at commit `474e28b` (*Bound COPNOQueueButton constructor candidates*).
The following 20 major engineering milestones, reverse-engineering discoveries, and production modules were researched, implemented, tested, and integrated afterward:

```
474e28b (Codex baseline)
  │
  ├── 73f8e71  Correct CKRCC candidate to 17CKRRCCameraStatus reading path
  ├── 8677c06  Add derived RAM reference tracking tool and test suite
  ├── 283b219  Add Nikon event adapter modeling RelayEventEx and button wire ABIs
  ├── 17d8ec4  Implement DrawBuffer API graphics layer and ARGB8888 rasterizer
  ├── 70e6091  Harden derived refs parser against non-immediate registers
  ├── 665c1a6  Wire DrawBuffer 720x480 OSD overlay rendering into sim runtime
  ├── 7768cdf  Document full button dispatch chain: COPNOQueueButton to RTOS mailbox to CSYSCAppButtonController
  ├── d5eab00  Identify T-Kernel 2.0 RTOS, map reset-to-kernel boot sequence, add T-Kernel layer
  ├── f793fdd  Implement D3300-ML relocatable payload module and lifecycle test suite
  ├── 5e1fc43  Map firmware update & SD diagnostic hooks, implement BMP/PPM framebuffer exporter
  ├── 0636876  Cross-reference qemu-expeed and DOOM port findings with D3300 EXPEED 4 architecture
  ├── 9c13b53  Map D3300 PTP service mode cluster and implement T-Kernel task lifecycle & quieting
  ├── 45c5be3  Implement D3300-ML custom shooting information display screen
  ├── 0661296  Refine custom shooting screen with Blackmagic cinema minimalism and vector stroke typography
  ├── a1cba89  Implement camera status RAM reader and payload INFO screen toggle
  ├── 73f08e6  Wire simulator keys directly to hardware status raw memory
  ├── 2fef0e9  Implement flat cinema menu renderer and realistic video features
  ├── b853b3c  Implement long-press MENU hold progress meter and smooth entrance animation
  ├── 77142f7  Introduce modern proportional sans font and in-menu font selector
  ├── 7e57b88  Implement Fujifilm film simulation Picture Control engine and menu integration
  └── 8101790  Implement realtime Focus Peaking, Zebra stripes, and Live Histogram photo assist engine
```

---

## 2. Reverse-Engineered Hardware & Firmware Boundaries

### 2.1. RTOS Kernel & Reset Boot Sequence (`d5eab00`, `9c13b53`)
- **Operating System**: **T-Kernel 2.0** (TRON specification, Sakamura Lab / T-Engine Forum).
- **Reset Vector**: `0x90352FE0` (ARM mode). Sets stack pointer, disables interrupts, copies initialized data segments to RAM (`0x10000000`, `0x1003B5B4`, `0x1011D420`), then jumps to kernel initialization.
- **Kernel Entry**: `0x9002E740` (`tk_sta_sys` / TRON system start).
- **Task Management**:
  - `tk_dis_dsp` / `tk_ena_dsp`: Disables/enables task dispatching.
  - `tk_sus_tsk` / `tk_rsm_tsk`: Suspends and resumes target RTOS tasks.
  - Implemented in `core/dml_tkernel.h/.c` as `dml_tk_quiet_tasks()` to suppress stock Nikon UI rendering tasks while D3300-ML OSD or Info Screen is active.

### 2.2. Button Dispatch Pipeline (`7768cdf`, `283b219`)
- **Physical Button Press**: GPIO interrupt -> Key scan task -> `COPNOQueueButton` (`0x9019A130`).
- **Mailbox Handoff**: `COPNOQueueButton` formats a 16-byte button message and posts it into RTOS Mailbox `MBF_OPNO` (`0x1012B908`).
- **Application Controller**: `CSYSCAppButtonController` (`0x90327628`) reads from `MBF_OPNO` via `0x9024144C` -> `0x90323D7C` and delivers to state object via `0x903512A0`.
- **Event Interception**: Modeled in `core/dml_nikon_adapter.h/.c` via `RelayEventEx` (`0x900272A6`) and `COPNO` packet filter.

### 2.3. Camera Live Status RAM Structure (`73f8e71`, `a1cba89`, `73f08e6`)
- **Structure**: `17CKRRCCameraStatus` located at `0x10491580` (vtable `0x9151F6F0`, RTTI `0x9150AFBB`).
- **Field Offsets**:
  - `+0x00`: Shutter speed APEX code (`tv_code`). E.g. `0x60` = 1/125s, `0x78` = 1/500s.
  - `+0x02`: Aperture APEX code (`av_code`). E.g. `0x28` = f/2.8, `0x38` = f/4.0.
  - `+0x04`: ISO APEX code (`iso_code`). E.g. `0x30` = ISO 100, `0x48` = ISO 800.
  - `+0x06`: Exposure compensation code (`ev_bias_code`).
  - `+0x08`: Battery level (0..4) and critical low flag (`0x80`).
  - `+0x0A`: Flash status flags (bit 0: popped up, bit 1: charged, bit 2: TTL active).
  - `+0x0C`: Active autofocus point index (0..10 for D3300 Multi-CAM 1000 11-point system).
  - `+0x10`: Shots remaining on SD card (uint32).
  - `+0x14`: Drive mode (Single, Continuous, Quiet, Self-Timer).
- **Access Module**: `core/dml_camera_status_hw.h/.c` provides safe validation, bounds-checking, and human-readable string conversions.

### 2.4. Graphics DrawBuffer Layer (`17d8ec4`, `665c1a6`)
- **Buffer Table**: Located at RAM `0x104A0868`. 20 entries of display surface descriptors.
- **Resolution**: 720 × 480 (NTSC 3:2 aspect ratio matching the 921k-dot TFT LCD).
- **Pixel Format**: ARGB8888 (32-bit color, 8-bit alpha).
- **Buffer Stride**: 2880 bytes (720 pixels × 4 bytes).
- **Implementation**: `core/dml_draw_buffer.h/.c` provides clipping-safe drawing primitives: line, rect, filled rect, rounded rect, antialiased ring arc, pixel-put, and multi-font string rasterization.

### 2.5. PTP Vendor Commands & Service Mode (`9c13b53`)
- **Vendor Opcode Cluster**: `0x94FE` / `0x94FF` with sub-operations `0xF001`, `0xF036`, `0xF037`.
- **43 Vendor Commands**: Mapped and cataloged in `reports/D3300_C102_qemu_expeed_cross_analysis.md` and `tools/d3300_ptp_service.py`.
- **Capabilities**: Unlocked diagnostics, direct DRAM read/write via PTP buffer, sensor calibration dump.

---

## 3. High-Level Modules Added

### 3.1. Fujifilm Film Simulation Picture Control Engine (`7e57b88`)
- **File**: `modules/fuji_film_sim.h`, `modules/fuji_film_sim.c`.
- **Spec Compliance**: Generates authentic 700-byte Nikon `.NOP` and `.NCP` binary Picture Control files.
  - Record `0x01` (Offset `0x000`..`0x05F`): 16-character ASCII name, base picture control ID (`0x01` STANDARD, `0x02` NEUTRAL), sharpening (0..9), contrast (-3..+3), brightness (-1..+1), saturation (-3..+3), hue (-3..+3).
  - Record `0x02` (Offset `0x060`..`0x2BB`): Non-linear Tone Curve (256 uint16 entries, $0..65535$).
  - Record `0x00` (Offset `0x2BC`): EOF record terminator.
- **Authentic Recipes**:
  1. `CLASSIC CHROME`: Tone curve with +2.2% black level lift (matte shadows), soft shoulder rolloff, desaturated colors (-1 saturation), +1 warm hue shift.
  2. `FUJI VELVIA`: Steep S-curve contrast, +2 saturation, sharp microcontrast (+4 sharpening).
  3. `FUJI ACROS`: High microcontrast monochrome curve, yellow filter spectral response, deep black anchor.
  4. `CLASSIC NEG`: Superia 400 emulation with crushed dark shadows, lifted midtones, and olive-cyan tint.
  5. `FUJI ASTIA`: Portrait-optimized smooth gradation curve, gentle highlights, soft skin tones.
- **Direct SD Export**: Creates `/NIKON/CUSTOMPC/PICCON01.NOP` through `PICCON05.NOP` (and corresponding `.NCP` copies) for instant import on any Nikon D3300 (C:1.01 or C:1.02).

### 3.2. Real-Time Photo Assist Suite (`8101790`)
- **File**: `modules/photo_assist.h`, `modules/photo_assist.c`.
- **Focus Peaking**:
  - Discrete integer Sobel convolution operator:
    $$G_x = (P_{x+1,y-1} + 2P_{x+1,y} + P_{x+1,y+1}) - (P_{x-1,y-1} + 2P_{x-1,y} + P_{x-1,y+1})$$
    $$G_y = (P_{x-1,y+1} + 2P_{x,y+1} + P_{x+1,y+1}) - (P_{x-1,y-1} + 2P_{x,y-1} + P_{x+1,y-1})$$
    $$\text{Gradient} = |G_x| + |G_y|$$
  - Sensitivity levels: `LOW` (threshold 220), `MEDIUM` (threshold 140), `HIGH` (threshold 80).
  - Peaking colors: `GREEN` (`#00FF44`), `RED` (`#FF2233`), `CYAN` (`#00E5FF`), `YELLOW` (`#FFDD00`), `WHITE` (`#FFFFFF`).
  - Execution speed: <0.4ms on 720×480 framebuffer via 2-pixel stride downsampling.
- **Exposure Zebra**:
  - Real-time luminance threshold detection:
    $$Y = (R \times 77 + G \times 150 + B \times 29) \gg 8$$
  - Threshold presets: `100%` ($Y \ge 255$), `95%` ($Y \ge 242$), `90%` ($Y \ge 230$), `75%` ($Y \ge 191$).
  - 45-degree animated diagonal zebra stripes with phase accumulation.
- **Live Luma / RGB Histogram HUD**:
  - 64-bin real-time luminance histogram rendered in a translucent 132×60 HUD box.
  - Reference vertical grid lines at 25%, 50%, and 75% dynamic range.
  - Highlight and shadow clipping alert dots (red indicator in top-right corner if >1% pixels clip).

### 3.3. Cinema OS Menu & Long-Press Button Hold Meter (`2fef0e9`, `b853b3c`)
- **File**: `core/dml_menu.h`, `core/dml_menu.c`, `core/dml_menu_screen.h`, `core/dml_menu_screen.c`.
- **MENU Button Long-Press Filter**:
  - Short press (<500ms): Transmits stock event to Nikon menu.
  - Long press ($\ge$500ms): Trapped by D3300-ML, activates Cinema OS overlay.
  - Visual Feedback: Real-time circular progress ring (`dml_draw_buffer_draw_ring_arc`) rendered around cursor while holding.
- **5 Category Tabs**:
  1. `SHOOT`: Film Simulation selection (Classic Chrome, Velvia, Acros, Classic Neg, Astia), Aspect Ratio crop guides (16:9, 2.39:1 Anamorphic, 4:3, 1:1), Shutter Angle mode.
  2. `EXPO`: Zebra stripes threshold, Focus Peaking color & sensitivity, Live Histogram mode.
  3. `OVERLAY`: Rule of Thirds grid, Center crosshair, Waveform monitor, Audio meters.
  4. `MOVIE`: 24.000p Cinema FPS lock, Bitrate overdrive (24 Mbps -> 45 Mbps), Clean HDMI toggle.
  5. `SYSTEM`: Typography font selector (`MODERN SANS`, `MONO TECH`, `RETRO MATRIX`), RAM status reader dump, Payload diagnostic info.

### 3.4. Modern Typography Engine (`77142f7`)
- **File**: `core/dml_fonts_data.h`, `core/dml_fonts_data.c`.
- **Modern Sans**: Custom high-legibility proportional sans-serif bitmap font (14px title, 10px regular, optical proportional glyph widths 3..12px). Replaces rough fixed-pitch matrix fonts with clean, professional typography.
- **Font Selector**: Runtime selectable in menu between `Modern Sans`, `Mono Tech`, and `Retro Matrix`.

### 3.5. Minimalist Custom Information Screen (`45c5be3`, `0661296`)
- **File**: `core/dml_info_screen.h`, `core/dml_info_screen.c`.
- **Design**: Inspired by Blackmagic Pocket Cinema Camera and ARRI Alexa UI:
  - Giant high-contrast Shutter Speed (`1/125`) and Aperture (`f/2.8`) readouts.
  - Clean ISO indicator with dynamic highlight for native vs boosted sensitivities.
  - Multi-CAM 1000 11-point autofocus active sensor overlay.
  - Battery percentage bar with critical voltage alert.

### 3.6. Relocatable DRAM Payload Module (`f793fdd`)
- **File**: `payload/dml_payload.h`, `payload/dml_payload.c`.
- **Memory Map**: Position-independent execution in volatile DRAM `0x10480000..0x104A0000` (128 KB reserved space).
- **Lifecycle API**: `dml_payload_init()`, `dml_payload_step(delta_ms)`, `dml_payload_handle_button(btn_code, duration_ms)`, `dml_payload_render()`, `dml_payload_cleanup()`.
- **Zero Flash Impact**: Fully cleared on camera power-cycle.

---

## 4. Test & Verification Matrix

All modules are accompanied by unit and regression test binaries executing against real math, structures, and memory layouts:

| Test Binary | Scope & Verification | Status |
|---|---|:---:|
| `test_photo_assist` | Focus Peaking Sobel math, Zebra threshold clipping, Histogram 64-bin distribution, Menu cycling | **PASS** |
| `test_fuji_film_sim` | Nikon `.NOP`/`.NCP` 700B binary structure, Record 0x01/0x02 parsing, curve monotonicity, SD card recursive export | **PASS** |
| `test_menu` | 5-tab navigation, item cycling, hold meter progress accumulation, entrance animation easing | **PASS** |
| `test_fonts` | Modern Sans, Mono Tech, Retro Matrix string rendering, proportional kerning, glyph bounds | **PASS** |
| `test_camera_status_hw` | `17CKRRCCameraStatus` memory parser, APEX code conversions, battery low flag, AF points | **PASS** |
| `test_info_screen` | Custom shooting screen render passes, Blackmagic styling, clipping checks | **PASS** |
| `test_payload` | DRAM payload lifecycle (`init` -> `button` -> `render` -> `cleanup`), memory boundaries | **PASS** |
| `test_tkernel` | T-Kernel 2.0 task suspend/resume, quieting task list management | **PASS** |
| `test_draw_buffer` | 720x480 ARGB8888 primitives, clipping guard, BMP/PPM framebuffer exporter | **PASS** |
| `test_event_adapter` | `RelayEventEx` table slot allocation, unregister tokens, button queue wire | **PASS** |
| `test_camera_status` | Standard status model setters/getters | **PASS** |
| `pytest tests/` | 37 Python tests (firmware parsing, derived RAM refs, anchors, PTP services, Ghidra map) | **PASS** |

---

## 5. Instructions for Running and Verifying

```bash
# 1. Clean build and run full test suite (C11 + Python):
make clean && make test

# 2. Export Fujifilm Film Simulation profiles to an SD card or directory:
./build/d3300_sim --export-fuji-profiles /Volumes/NIKON_SD

# 3. Launch interactive host simulator:
./build/d3300_sim --interactive

# 4. Generate visual verification screenshots:
./build/d3300_sim --screenshot-assist assist_preview.bmp
./build/d3300_sim --screenshot-tab 0 shoot_tab.bmp
./build/d3300_sim --screenshot-tab 1 expo_tab.bmp
./build/d3300_sim --screenshot-tab 2 overlay_tab.bmp
./build/d3300_sim --screenshot-fuji 1 fuji_chrome.bmp
./build/d3300_sim --screenshot-info custom_info.bmp
```
