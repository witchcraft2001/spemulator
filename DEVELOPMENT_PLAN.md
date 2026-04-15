# SPEmulator — Sprinter (Peters Plus SP2000) Emulator Development Plan

## Overview

Cross-platform (macOS, Linux, Windows) lightweight emulator of the Sprinter SP2000 computer,
written in C with SDL3 for graphics/audio. Focus: cycle-accurate Z80 (Z84C15) emulation,
precise Sprinter architecture emulation, built-in debugger.

---

## Architecture Summary

```
┌──────────────────────────────────────────────────────┐
│                     Main Loop                         │
│  (frame timing, event polling, SDL3 window)           │
├──────────────────────────────────────────────────────┤
│                    Device Bus                         │
│  (device registry, port dispatch, memory dispatch)    │
├────────┬────────┬────────┬────────┬─────────────────┤
│  Z80   │ Memory │ Video  │ Audio  │  Peripherals    │
│ Z84C15 │  MMU   │  ULA   │AY/CBL  │ KBD/FDD/IDE... │
│  +CTC  │ +DCP   │        │Beeper  │                 │
│  +SIO  │        │        │        │                 │
│  +PIO  │        │        │        │                 │
├────────┴────────┴────────┴────────┴─────────────────┤
│                    Debugger                           │
│  (breakpoints, disasm, memory dump, step)             │
└──────────────────────────────────────────────────────┘
```

---

## Phase 1: Foundation & Z80 Core

### 1.1 Project Skeleton ✅
- [x] CMake build system (macOS/Linux/Windows)
- [x] Source directory structure
- [x] Main entry point, argument parsing
- [x] Configuration file parser (INI format)
- [x] SDL3 window creation, basic event loop

### 1.2 Z80 CPU Core
- [ ] Full Z80 instruction set (documented + undocumented)
- [ ] Correct T-state counting per instruction
- [ ] All addressing modes (immediate, register, indirect, indexed)
- [ ] Interrupt modes: IM0, IM1, IM2
- [ ] NMI handling
- [ ] HALT state with proper wake-up
- [ ] Undocumented instructions (SLL, IX/IY half-registers, etc.)
- [ ] Undocumented flags (bits 3,5 of F register, BLK instructions)
- [ ] R register accurate increment
- [ ] Memory refresh cycles
- [ ] MEMPTR/WZ internal register

### 1.3 Z84C15 Integrated Peripherals
- [ ] CTC (Counter/Timer Circuit) — 4 channels
  - Channel 0: baud rate for SIO-B
  - Channel 1: baud rate for SIO-B
  - Channel 2: system timer / keyboard clock
  - Channel 3: VSync interrupt source
- [ ] SIO (Serial I/O) — 2 channels
  - Channel A: PS/2 keyboard data
  - Channel B: RS-232 serial (mouse)
- [ ] PIO (Parallel I/O) — 2 ports (joystick, etc.)
- [ ] Interrupt daisy chain between CTC/SIO/PIO
- [ ] Watchdog Timer (basic)

### 1.4 Basic Memory System
- [ ] 4MB RAM (256 × 16KB pages)
- [ ] 512KB ROM/Flash (32 × 16KB pages)
- [ ] 4-window memory banking (ports #82, #A2, #C2, #E2)
- [ ] Default page mapping at reset (WIN0=ROM0, WIN1=RAM2, WIN2=RAM10, WIN3=RAM0)
- [ ] Memory read/write with page translation
- [ ] ROM write protection

---

## Phase 2: Sprinter Architecture

### 2.1 DCP (Dynamic Configuration Port) System
- [ ] 256×16-bit port lookup table (DCP.MIF equivalent)
- [ ] Port type decoding (TYPE field bits 15-12)
- [ ] Wait state generation per port type
- [ ] Port read/write dispatch to devices
- [ ] Device signal routing via DCP bits 11-0

### 2.2 Video System — ZX Spectrum Mode
- [ ] ZX 256×192 pixel mode
- [ ] Non-linear ZX pixel addressing
- [ ] 768-byte attribute area (8×8 color blocks)
- [ ] Border color (port #FE bits 2:0)
- [ ] FLASH attribute blinking (frame counter)
- [ ] SDL3 texture rendering from framebuffer
- [ ] Correct frame timing (50Hz PAL / 60Hz NTSC)

### 2.3 Video System — Sprinter Native Modes
- [ ] Mode 1: 320×256, 8bpp linear
  - Screen A (pages #50-#54) / Screen B (pages #55-#59)
  - Pixel format: 1 byte = 1 pixel index
- [ ] Mode 2: 640×256, 4bpp
  - 2 pixels per byte (high nibble = left)
  - 16 colors from palette
- [ ] Text mode: 80×32 (hardware character generation)
- [ ] PORT_Y (#89) — VRAM Y-coordinate for access
- [ ] RGMOD (#C9) — double buffer flip, screen on/off
- [ ] ALL_MODE (#C3) — video mode select
- [ ] 256-color palette (6-6-6 RGB, port #89 for index)

### 2.4 VRAM Page Modes
- [ ] Normal (pages #50-#53): VRAM + DRAM write
- [ ] VRAM-only (#54-#57): VRAM only, DRAM unchanged
- [ ] Transparent (#58-#5B): skip #FF bytes
- [ ] Sprite mode (#5C-#5F): transparent + VRAM-only

### 2.5 Hardware Accelerator (Blitter)
- [ ] Intercept LD r,r where src==dst during execution
- [ ] LD D,D (#52): set block size from A
- [ ] LD C,C (#49): fill block (HL=addr, A=value)
- [ ] LD E,E (#5B): vertical fill (HL=addr, A=height)
- [ ] LD L,L (#6D): copy row (HL→DE, A=length)
- [ ] LD A,A (#7F): copy vertical (HL→DE, A=height)
- [ ] LD B,B (#40): disable accelerator
- [ ] 42MHz operation (independent of CPU turbo)
- [ ] Correct T-state accounting for accelerated ops

### 2.6 Turbo Mode
- [ ] Normal: 7MHz (42/6)
- [ ] Turbo: 21MHz (42/2)
- [ ] Wait state table per access type and speed mode
- [ ] Clock switch via port or hotkey

---

## Phase 3: BIOS Boot & Basic I/O

### 3.1 ROM Loading
- [ ] Load ROM image from file (command line or config)
- [ ] Map ROM to pages #80-#FF
- [ ] Reset vector at 0x0000 (ROM page 0 in WIN0)
- [ ] RST #08 handler (BIOS API entry)
- [ ] RST #38 handler (VSync interrupt)

### 3.2 Keyboard Input
- [ ] ZX matrix keyboard emulation (port #FE, A8-A15 row select)
- [ ] PS/2 keyboard via SIO-A (port #18/#19)
- [ ] SDL3 key event → ZX matrix mapping
- [ ] SDL3 key event → PS/2 scancode mapping
- [ ] Key repeat handling

### 3.3 CMOS/RTC
- [ ] DS12885 RTC emulation (ports #1C/#1D/#1E)
- [ ] Time/date registers from host system
- [ ] CMOS configuration registers (#0E-#1B)
- [ ] Boot device selection (CMOS #10)
- [ ] Turbo/config settings (CMOS #1B)

### 3.4 BIOS Boot Sequence Support
- [ ] BIOS initialization (KINIT, ZXCLS, CMOS read)
- [ ] SETUP detection (DEL key at boot)
- [ ] Boot device probing (AUTOIDE)
- [ ] DSS loading from disk image

---

## Phase 4: Storage Devices

### 4.1 IDE/ATA Controller
- [ ] IDE register set (read ports #0050-#0055, write #0150-#0155)
- [ ] Status register (#4053) with BSY/RDY/DRQ/ERR flags
- [ ] ATA commands: IDENTIFY, READ SECTOR(S), WRITE SECTOR(S)
- [ ] Device/Head register (master/slave select)
- [ ] 16-bit data transfer via port #0050/#0150
- [ ] CHS and LBA addressing modes
- [ ] Image file backends: raw .img, .chd (CHD v5 decompression)
- [ ] Wait states: 4 (data), 10 (registers)

### 4.2 ATAPI/CD-ROM
- [ ] ATAPI IDENTIFY
- [ ] PACKET command interface
- [ ] READ(10) for sector access
- [ ] .iso image file backend

### 4.3 Floppy Disk Controller (WD1793)
- [ ] WD1793 register set (ports via MAX7000)
- [ ] Commands: RESTORE, SEEK, STEP, READ SECTOR, WRITE SECTOR
- [ ] Status register (BUSY, DRQ, INDEX, etc.)
- [ ] Motor control, head stepping
- [ ] Disk image backend (.img raw sector images)
- [ ] Beta Disk interface compatibility

### 4.4 Image File Support
- [ ] Raw sector images (.img) — direct file offset mapping
- [ ] CHD images (.chd) — MAME compressed hard disk (v5)
- [ ] ISO images (.iso) — CD-ROM sector images
- [ ] Partition table / MBR reading

---

## Phase 5: Audio System

### 5.1 AY-3-8910 Sound Chip
- [ ] Register file (R0-R15)
- [ ] 3 tone channels (A, B, C) with period control
- [ ] Noise generator with period
- [ ] Mixer register (tone/noise enable per channel)
- [ ] Amplitude control (fixed + envelope)
- [ ] Envelope generator (period + shape)
- [ ] Ports: #8D (register select), #8E (data)
- [ ] Legacy ports: #FFFD / #BFFD
- [ ] SDL3 audio output (mixing with other sources)

### 5.2 Covox Blaster (PCM DAC)
- [ ] Sample write (port #88)
- [ ] Mode register (port #89): enable, stereo, 16-bit, freq
- [ ] Frequency codes (#0 through #F: 7.8kHz – 109kHz)
- [ ] DMA playback with interrupt
- [ ] Mono and stereo output

### 5.3 Beeper
- [ ] Port #FE bit 4 — square wave toggle
- [ ] Mix into audio output

---

## Phase 6: Debugger

### 6.1 Core Debugger Features
- [ ] Break on PC address (breakpoint list)
- [ ] Break on memory read/write (watchpoints)
- [ ] Break on port I/O
- [ ] Single step (instruction)
- [ ] Step over (skip CALL/RST)
- [ ] Run to address
- [ ] CPU register display and edit

### 6.2 Disassembler
- [ ] Full Z80 disassembly (including CB/ED/DD/FD prefixes)
- [ ] Undocumented instruction mnemonics
- [ ] Symbol/label support (optional)
- [ ] Disassembly window following PC

### 6.3 Memory Inspector
- [ ] Hex dump view of any memory page (0x00-0xFF)
- [ ] Logical (windowed) and physical (paged) view
- [ ] VRAM viewer
- [ ] Search in memory (byte pattern)
- [ ] Memory edit

### 6.4 Debugger UI
- [ ] Option A: Separate SDL3 window with text rendering
- [ ] Option B: Terminal-based (ncurses / raw terminal)
- [ ] Hotkey to toggle debugger (e.g., F12)
- [ ] Register view, disassembly view, memory view, stack view
- [ ] Breakpoint management panel

---

## Phase 7: Advanced Features

### 7.1 Mouse Support
- [ ] Kempston mouse protocol
- [ ] RS-232 serial mouse via SIO-B
- [ ] SDL3 mouse capture and relative movement

### 7.2 Joystick
- [ ] Kempston joystick (port #1F)
- [ ] PIO-based joystick
- [ ] SDL3 gamepad mapping

### 7.3 ISA Bus Stub
- [ ] Port #9FBD (ISA port register)
- [ ] Memory window at #C000
- [ ] Stub for future expansion cards

### 7.4 Save/Load State
- [ ] Full machine state serialization
- [ ] Save to file / load from file
- [ ] Quick save/load hotkeys

### 7.5 Additional Features
- [ ] Screenshot (PNG export)
- [ ] Tape loading support (if applicable)
- [ ] Speed control (1x, 2x, unlimited)
- [ ] Display scaling (1x, 2x, 3x, fullscreen)

---

## Phase 8: Polish & Testing

### 8.1 CPU Test Suite
- [ ] ZEXALL / ZEXDOC conformance tests
- [ ] Undocumented instruction tests
- [ ] Interrupt timing tests
- [ ] CTC/SIO timing tests

### 8.2 Software Compatibility
- [ ] DSS boot and shell
- [ ] FlexNavigator file manager
- [ ] Demo programs from tutorials
- [ ] Game: Thunder in the Deep
- [ ] Various .SPE executables

### 8.3 Performance
- [ ] Profile and optimize hot paths
- [ ] Ensure 60fps at 1x speed on all platforms
- [ ] Memory usage audit

---

## File Structure

```
spemulator/
├── CMakeLists.txt
├── DEVELOPMENT_PLAN.md
├── README.md
├── config/
│   └── default.ini          # Default configuration
├── src/
│   ├── main.c               # Entry point, argument parsing
│   ├── config.c/h           # INI config parser
│   ├── machine.c/h          # Machine state, main loop
│   ├── bus.c/h              # Device bus, port/memory dispatch
│   ├── cpu/
│   │   ├── z80.c/h          # Z80 core (registers, exec, interrupts)
│   │   ├── z80_ops.c        # Opcode implementations
│   │   ├── z80_cb.c         # CB-prefixed opcodes
│   │   ├── z80_ed.c         # ED-prefixed opcodes
│   │   ├── z80_dd.c         # DD/FD-prefixed opcodes (IX/IY)
│   │   ├── z80_ddcb.c       # DD CB / FD CB opcodes
│   │   ├── z80_disasm.c/h   # Disassembler
│   │   ├── z80_tables.c     # Flag/timing lookup tables
│   │   ├── ctc.c/h          # Z80-CTC emulation
│   │   ├── sio.c/h          # Z80-SIO emulation
│   │   └── pio.c/h          # Z80-PIO emulation
│   ├── memory/
│   │   ├── mmu.c/h          # Memory management unit
│   │   └── dcp.c/h          # DCP port decoder
│   ├── video/
│   │   ├── video.c/h        # Video controller
│   │   ├── palette.c/h      # Palette management
│   │   └── accel.c/h        # Hardware accelerator (blitter)
│   ├── audio/
│   │   ├── audio.c/h        # Audio mixer, SDL3 audio
│   │   ├── ay8910.c/h       # AY-3-8910 sound chip
│   │   ├── covox.c/h        # Covox Blaster DAC
│   │   └── beeper.c/h       # Beeper
│   ├── disk/
│   │   ├── ide.c/h          # IDE/ATA controller
│   │   ├── fdd.c/h          # Floppy (WD1793)
│   │   ├── atapi.c/h        # ATAPI/CD-ROM
│   │   └── image.c/h        # Disk image backends (.img/.chd/.iso)
│   ├── input/
│   │   ├── keyboard.c/h     # Keyboard (ZX matrix + PS/2)
│   │   ├── mouse.c/h        # Mouse (Kempston + serial)
│   │   └── joystick.c/h     # Joystick
│   ├── periph/
│   │   ├── rtc.c/h          # DS12885 RTC / CMOS
│   │   └── isa.c/h          # ISA bus stub
│   ├── debug/
│   │   ├── debugger.c/h     # Debugger core
│   │   ├── breakpoint.c/h   # Breakpoint management
│   │   └── dbg_ui.c/h       # Debugger UI (SDL3 or terminal)
│   └── platform/
│       ├── platform.c/h     # Platform abstraction
│       └── sdl_backend.c/h  # SDL3 window, rendering, audio
└── roms/                     # ROM files (not in repo)
```

---

## Configuration File Format (INI)

```ini
[general]
; CPU speed: 7 or 21 (MHz)
cpu_speed = 7
; RAM size in KB (512, 1024, 2048, 4096)
ram_size = 4096

[rom]
; Path to BIOS ROM image (512KB)
bios = /path/to/sprinter.rom

[video]
; Window scale: 1, 2, 3
scale = 2
; Fullscreen: 0 or 1
fullscreen = 0

[disk]
; Floppy drive A image
fdd0 = /path/to/floppy.img
; Floppy drive B image
fdd1 =
; IDE master image (HDD)
ide0 = /path/to/hdd.img
; IDE slave image (HDD or CDROM)
ide1 =
; ATAPI CD-ROM image
cdrom = /path/to/cd.iso

[audio]
; Enable audio: 0 or 1
enabled = 1
; Sample rate
sample_rate = 44100

[debug]
; Start with debugger open: 0 or 1
debugger = 0
```

---

## Command Line Options

```
spemulator [options]

Options:
  --rom <file>        BIOS ROM image
  --fdd0 <file>       Floppy drive A image
  --fdd1 <file>       Floppy drive B image
  --ide0 <file>       IDE master (HDD) image
  --ide1 <file>       IDE slave image
  --cdrom <file>      CD-ROM ISO image
  --config <file>     Configuration file path
  --scale <1|2|3>     Window scale factor
  --fullscreen        Start in fullscreen
  --debug             Start with debugger open
  --turbo             Start in turbo mode (21MHz)
  --help              Show help
```

---

## Priority Order for Implementation

1. **Phase 1.1** — Project skeleton, build, SDL3 window → immediate
2. **Phase 1.2** — Z80 CPU core → foundation for everything
3. **Phase 1.4** — Basic memory system → needed for CPU testing
4. **Phase 2.2** — ZX video mode → first visual output
5. **Phase 3.1** — ROM loading → BIOS boot attempt
6. **Phase 3.2** — Keyboard → interact with BIOS
7. **Phase 2.1** — DCP system → proper port routing
8. **Phase 1.3** — Z84C15 peripherals (CTC/SIO) → interrupts, keyboard
9. **Phase 2.3** — Native video modes → Sprinter software display
10. **Phase 2.5** — Accelerator → performance-critical for Sprinter software
11. **Phase 4.1** — IDE controller → boot from HDD
12. **Phase 4.3** — FDD controller → boot from floppy
13. **Phase 6** — Debugger → essential for development/testing
14. **Phase 5** — Audio → not blocking, enhances experience
15. **Phase 3.3** — RTC/CMOS → system configuration
16. **Phase 7** — Advanced features → polish
17. **Phase 8** — Testing & conformance → quality assurance
