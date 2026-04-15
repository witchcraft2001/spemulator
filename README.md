# SPEmulator — Sprinter SP2000 Emulator

Emulator for the **Sprinter SP2000** computer — a Z80-based machine (Peters Plus Ltd, Russia) with extended graphics, DMA, 4MB RAM, IDE/FDD, and ISA expansion.

## Features

- **Z80 CPU (Z84C15)** — full instruction set including undocumented opcodes (IXH/IXL, SLL, etc.), cycle-accurate timing, MEMPTR (WZ) register
- **Two-phase boot** — config loader (FPGA bitstream) followed by BIOS cold start, matching real hardware behavior
- **DCP-based port decoding** — Dynamic Configuration Port architecture for I/O routing
- **Memory management** — 4 x 16KB memory windows with page registers, ROM/RAM/FastRAM switching, VRAM PORT_Y addressing
- **DS12887 RTC/CMOS** — real-time clock with BCD time, status registers, battery-OK flag, ISA port access (#DFBD/#BFBD/#FFBD)
- **Video** — tile-based renderer with 4-byte mode descriptors, 8 palette banks x 256 colors, 320x256/640x256/ZX-compatible modes
- **Hardware accelerator (blitter)** — 42MHz block fill/copy via LD r,r opcode interception
- **Audio** — AY-3-8910, Covox stereo DAC, beeper
- **Peripherals** — CTC (4 channels), SIO (2 channels), IDE/ATA controller, FDD (WD1793 stub)
- **Built-in debugger** — breakpoints (exec/mem/port), step/step-over, register display, disassembly
- **Trace logging** — runtime I/O/memory/boot tracing to stderr, configurable categories, zero overhead in release builds
- **Cross-platform** — macOS, Linux, Windows (SDL3)

## Building

Requirements: CMake 3.16+, SDL3, C11 compiler.

```bash
mkdir build && cd build

# Release build (no trace overhead):
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build (trace logging enabled):
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build WITH trace logging:
cmake -DCMAKE_BUILD_TYPE=Release -DSPEMU_ENABLE_TRACE=ON ..

make -j$(nproc)
```

## Usage

```bash
./spemulator --rom <bios.rom> [options]
```

### Options

| Option | Description |
|--------|-------------|
| `--rom <file>` | BIOS ROM image (256KB) |
| `--fdd0 <file>` | Floppy drive A image |
| `--ide0 <file>` | IDE master (HDD) image |
| `--scale <1\|2\|3>` | Window scale factor (default: 2) |
| `--fullscreen` | Start in fullscreen mode |
| `--turbo` | Start in turbo mode (21 MHz) |
| `--debug` | Start with debugger open |
| `--trace [spec]` | Enable trace logging to stderr |
| `--help` | Show all options |

### Trace Logging

Trace output goes to stderr, pipe-friendly:

```bash
# Default trace (ports, pages, IRQ, boot, CMOS):
./spemulator --rom rom.bin --trace 2>trace.log

# Specific categories:
./spemulator --rom rom.bin --trace "io,cmos" 2>trace.log

# All categories:
./spemulator --rom rom.bin --trace all 2>trace.log

# Filter with grep:
./spemulator --rom rom.bin --trace io 2>&1 >/dev/null | grep "PW.*1C"
```

Categories: `port` (`pr`, `pw`), `mem` (`mr`, `mw`), `page`, `irq`, `boot`, `cmos`, `cpu`, `ctc`, `video`, `io`, `default`, `verbose`, `all`.

### Controls

| Key | Action |
|-----|--------|
| F11 | Toggle fullscreen |
| F12 | Toggle debugger |
| Ctrl+F10 | Quit |

## Architecture

```
src/
  main.c              — Main loop, SDL event processing
  machine.{h,c}       — Machine state, memory/port I/O, update_memory()
  config.{h,c}        — Configuration and CLI parsing
  bus.{h,c}           — Device bus infrastructure
  types.h             — Common types (u8, u16, u32, u64)
  cpu/
    z80.{h,c}         — Z80 core: registers, step, IRQ/NMI
    z80_ops.c          — Unprefixed opcodes + accelerator hooks
    z80_cb.c           — CB prefix (rotate/shift/bit)
    z80_ed.c           — ED prefix (block ops, IN/OUT, 16-bit ALU)
    z80_dd.c           — DD/FD prefix (IX/IY)
    z80_ddcb.c         — DDCB/FDCB (bit ops on IX/IY+d)
    z80_disasm.c       — Full Z80 disassembler
    ctc.{h,c}          — Z80-CTC (4 channels, timer/counter)
    sio.{h,c}          — Z80-SIO (2 channels)
  memory/
    mmu.{h,c}          — MMU init/reset (delegates to update_memory)
    dcp.{h,c}          — DCP port decode infrastructure
  video/
    video.{h,c}        — Tile-based renderer (mode descriptors, symbols)
    palette.{h,c}      — 8-bank palette (2048 ARGB entries)
    accel.{h,c}        — Hardware accelerator (blitter)
  audio/
    audio.{h,c}        — Audio mixing
    ay8910.{h,c}       — AY-3-8910 sound chip
    covox.{h,c}        — Covox DAC
    beeper.{h,c}       — Beeper output
  disk/
    ide.{h,c}          — IDE/ATA controller
    fdd.{h,c}          — FDD controller (WD1793 stub)
  periph/
    rtc.{h,c}          — DS12887 RTC/CMOS (BCD time, status registers)
    isa.{h,c}          — ISA bus stub
  input/
    keyboard.{h,c}     — Keyboard (ZX matrix)
    mouse.{h,c}        — Mouse
    joystick.{h,c}     — Joystick
  debug/
    debugger.{h,c}     — Interactive debugger
    breakpoint.{h,c}   — Breakpoint manager
    trace.{h,c}        — Trace logging system
  platform/
    sdl_backend.{h,c}  — SDL3 window, texture, audio
    platform.{h,c}     — Platform detection
```

## Sprinter Hardware Overview

The Sprinter SP2000 is a Z84C15-based computer running at 7/21 MHz with:
- **DCP (Dynamic Configuration Port)** — 256x16-bit FPGA LUT that decodes all I/O ports. Index computed from CNF, PN, DOS, R/W direction, and address bits.
- **Memory** — 4MB RAM (256 x 16KB pages), 256KB ROM (16 pages), 256KB VRAM (256 x 1KB lines), 64KB FastRAM
- **VRAM** — accessed via PORT_Y register: `address = PORT_Y * 1024 + (offset & 0x3FF)`. Each 1KB line has mode descriptors at offset 0x300 and palette data at 0x3E0+.
- **Video tiles** — 16x8 pixels. 4-byte descriptors select tile/symbol mode, palette bank, 8bpp/4bpp rendering.
- **Boot sequence** — Phase 1: config loader at ROM page 0x0C writes FPGA bitstream. After 4096+ bytes, soft reset triggers Phase 2: BIOS at ROM page 8.

## Status

- Z80 CPU: complete (all prefixes, undocumented instructions, cycle timing)
- Boot: Phase 1 and Phase 2 working, BIOS reaches main initialization
- CMOS/RTC: working (both DCP ports and ISA ports)
- Memory tests: passing
- Port register tests: passing
- Video output: framework implemented, needs BIOS to complete init
- DCP port routing: simplified (full DCP table not yet implemented)
- Disk: IDE/FDD stubs, no bootable media support yet

## References

- [MAME sprinter.cpp](https://github.com/mamedev/mame) — reference accuracy implementation
- Sprinter FPGA sources (sp-altera-src) — DCP.MIF, DCP.TDF
- BIOS v2.17 sources — assembly code for CMOS, video, boot routines
- DS12887 datasheet — RTC register map and timing

## License

MIT
