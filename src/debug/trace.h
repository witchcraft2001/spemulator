/*
 * SPEmulator — Trace/Logging System
 *
 * Runtime trace logging for debugging BIOS boot and device interaction.
 * Outputs to stderr in pipe-friendly format.
 *
 * Compile with -DSPEMU_TRACE to enable trace infrastructure.
 * Use --trace flag at runtime to activate.
 */
#ifndef SPEMU_TRACE_H
#define SPEMU_TRACE_H

#include "types.h"
#include <stdio.h>

/* Trace categories (bitmask) */
#define TR_PORT_R   0x0001  /* Port reads */
#define TR_PORT_W   0x0002  /* Port writes */
#define TR_MEM_R    0x0004  /* Memory reads (verbose!) */
#define TR_MEM_W    0x0008  /* Memory writes */
#define TR_PAGE     0x0010  /* Page/memory mapping changes */
#define TR_IRQ      0x0020  /* Interrupts */
#define TR_BOOT     0x0040  /* Boot sequence events */
#define TR_CMOS     0x0080  /* CMOS/RTC access */
#define TR_CPU      0x0100  /* CPU state (per instruction — very verbose) */
#define TR_CTC      0x0200  /* CTC events */
#define TR_VIDEO    0x0400  /* Video events */

/* Convenience groups */
#define TR_IO       (TR_PORT_R | TR_PORT_W)
#define TR_ALL_MEM  (TR_MEM_R | TR_MEM_W)
#define TR_DEFAULT  (TR_IO | TR_PAGE | TR_IRQ | TR_BOOT | TR_CMOS)
#define TR_VERBOSE  (TR_DEFAULT | TR_MEM_W | TR_CPU)
#define TR_ALL      0xFFFF

typedef struct {
    FILE *out;          /* Output stream (stderr by default) */
    u32   mask;         /* Active trace categories */
    bool  enabled;      /* Master enable */
    u64   start_ts;     /* Start logging after this T-state */
    u64   stop_ts;      /* Stop logging after this T-state (0=never) */
    u32   frame_start;  /* Start logging at this frame */
    u32   frame_stop;   /* Stop at this frame (0=never) */
} sp_trace_t;

/* Global trace state */
extern sp_trace_t g_trace;

/* Initialize trace system */
void trace_init(sp_trace_t *tr);

/* Parse trace spec string: "port,page,irq" or "all" or "default" */
void trace_parse_spec(sp_trace_t *tr, const char *spec);

/* Core logging function */
void trace_log(u32 category, u64 tstates, u16 pc, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* Convenience macros — compile out completely without SPEMU_TRACE */
#ifdef SPEMU_TRACE

#define TRACE(cat, ts, pc, ...) \
    do { if (g_trace.enabled && (g_trace.mask & (cat))) \
        trace_log((cat), (ts), (pc), __VA_ARGS__); } while(0)

#define TRACE_PORT_R(ts, pc, port, val) \
    TRACE(TR_PORT_R, ts, pc, "PR %04X=%02X", (unsigned)(port), (unsigned)(val))

#define TRACE_PORT_W(ts, pc, port, val) \
    TRACE(TR_PORT_W, ts, pc, "PW %04X=%02X", (unsigned)(port), (unsigned)(val))

#define TRACE_MEM_W(ts, pc, addr, val) \
    TRACE(TR_MEM_W, ts, pc, "MW %04X=%02X", (unsigned)(addr), (unsigned)(val))

#define TRACE_PAGE(ts, pc, win, page, rw) \
    TRACE(TR_PAGE, ts, pc, "PG WIN%d=%02X %s", (win), (unsigned)(page), (rw))

#define TRACE_IRQ(ts, pc, vec) \
    TRACE(TR_IRQ, ts, pc, "IRQ vec=%02X", (unsigned)(vec))

#define TRACE_CMOS(ts, pc, ...) \
    TRACE(TR_CMOS, ts, pc, __VA_ARGS__)

#define TRACE_BOOT(ts, pc, ...) \
    TRACE(TR_BOOT, ts, pc, __VA_ARGS__)

#define TRACE_CPU(ts, pc, ...) \
    TRACE(TR_CPU, ts, pc, __VA_ARGS__)

#else /* !SPEMU_TRACE */

#define TRACE(...)          ((void)0)
#define TRACE_PORT_R(...)   ((void)0)
#define TRACE_PORT_W(...)   ((void)0)
#define TRACE_MEM_W(...)    ((void)0)
#define TRACE_PAGE(...)     ((void)0)
#define TRACE_IRQ(...)      ((void)0)
#define TRACE_CMOS(...)     ((void)0)
#define TRACE_BOOT(...)     ((void)0)
#define TRACE_CPU(...)      ((void)0)

#endif /* SPEMU_TRACE */

#endif /* SPEMU_TRACE_H */
