/*
 * SPEmulator — Sprinter SP2000 Emulator
 * Common types and definitions
 */
#ifndef SPEMU_TYPES_H
#define SPEMU_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

/* Sprinter system constants */
#define SP_XTAL_HZ       42000000   /* 42 MHz master crystal */
#define SP_CPU_NORMAL_HZ   7000000  /* 42/6 = 7 MHz */
#define SP_CPU_TURBO_HZ   21000000  /* 42/2 = 21 MHz */
#define SP_FRAME_RATE_PAL       50
#define SP_FRAME_RATE_NTSC      60

#define SP_PAGE_SIZE       0x4000   /* 16KB per page */
#define SP_RAM_PAGES          256   /* 4MB = 256 * 16KB */
#define SP_ROM_PAGES           32   /* 512KB ROM/Flash */
#define SP_VRAM_SIZE      0x40000   /* 256KB VRAM */
#define SP_RAM_SIZE      (SP_RAM_PAGES * SP_PAGE_SIZE)  /* 4MB */
#define SP_ROM_SIZE      (SP_ROM_PAGES * SP_PAGE_SIZE)  /* 512KB */

/* Memory windows */
#define SP_WIN0_BASE    0x0000
#define SP_WIN1_BASE    0x4000
#define SP_WIN2_BASE    0x8000
#define SP_WIN3_BASE    0xC000

/* Page register ports */
#define SP_PORT_PAGE0   0x82
#define SP_PORT_PAGE1   0xA2
#define SP_PORT_PAGE2   0xC2
#define SP_PORT_PAGE3   0xE2

/* Video ports */
#define SP_PORT_Y       0x89
#define SP_PORT_ALLMODE 0xC3
#define SP_PORT_RGMOD   0xC9

/* Video modes */
#define SP_VMODE_ZX     0   /* 256x192, ZX Spectrum compat */
#define SP_VMODE_320    1   /* 320x256, 8bpp */
#define SP_VMODE_640    2   /* 640x256, 4bpp */

/* Video dimensions */
#define SP_SCREEN_W_ZX   256
#define SP_SCREEN_H_ZX   192
#define SP_SCREEN_W_320  320
#define SP_SCREEN_H_320  256
#define SP_SCREEN_W_640  640
#define SP_SCREEN_H_640  256
#define SP_BORDER_SIZE    32

/* Audio */
#define SP_PORT_AY_REG  0x8D
#define SP_PORT_AY_DAT  0x8E
#define SP_PORT_CBL_WR  0x88
#define SP_PORT_CBL_MODE 0x89

/* IDE ports (read) */
#define SP_PORT_IDE_DATA_R   0x0050
#define SP_PORT_IDE_ERR      0x0051
#define SP_PORT_IDE_SCOUNT   0x0052
#define SP_PORT_IDE_SNUM     0x0053
#define SP_PORT_IDE_CYLLO    0x0054
#define SP_PORT_IDE_CYLHI    0x0055
#define SP_PORT_IDE_STATUS   0x4053

/* IDE ports (write) */
#define SP_PORT_IDE_DATA_W   0x0150
#define SP_PORT_IDE_FEAT     0x0151
#define SP_PORT_IDE_SCOUNT_W 0x0152
#define SP_PORT_IDE_SNUM_W   0x0153
#define SP_PORT_IDE_CYLLO_W  0x0154
#define SP_PORT_IDE_CYLHI_W  0x0155

/* Keyboard */
#define SP_PORT_KBD_DATA  0x18
#define SP_PORT_KBD_STAT  0x19
#define SP_PORT_FE        0xFE

/* RTC/CMOS */
#define SP_PORT_CMOS_RD   0x1C
#define SP_PORT_CMOS_ADDR 0x1D
#define SP_PORT_CMOS_WR   0x1E

/* CTC ports (Z84C15 internal) */
#define SP_PORT_CTC0      0x10
#define SP_PORT_CTC1      0x11
#define SP_PORT_CTC2      0x12
#define SP_PORT_CTC3      0x13

#endif /* SPEMU_TYPES_H */
