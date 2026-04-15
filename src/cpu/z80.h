/*
 * SPEmulator — Z80 CPU Core
 * Cycle-accurate Z80 emulation with undocumented instructions and flags.
 * Includes MEMPTR (WZ) register.
 */
#ifndef SPEMU_Z80_H
#define SPEMU_Z80_H

#include "types.h"

/* Register pair union for easy hi/lo access */
typedef union {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    struct { u8 lo, hi; };
#else
    struct { u8 hi, lo; };
#endif
    u16 w;
} z80_pair_t;

/* Z80 CPU state */
typedef struct {
    /* Main registers */
    z80_pair_t af, bc, de, hl;
    /* Alternate registers */
    z80_pair_t af2, bc2, de2, hl2;
    /* Index registers */
    z80_pair_t ix, iy;
    /* Stack pointer and program counter */
    z80_pair_t sp, pc;
    /* MEMPTR (WZ) internal register */
    z80_pair_t wz;
    /* Interrupt vector, refresh counter */
    u8 i, r;
    /* R bit 7 (preserved separately since R increments only low 7 bits) */
    u8 r7;
    /* Interrupt flip-flops and mode */
    u8 iff1, iff2, im;
    /* CPU state flags */
    bool halted;
    bool nmi_pending;
    bool irq_pending;
    u8   irq_vector;  /* vector for IM0/IM2 */
    /* EI delay: after EI, interrupts enabled after next instruction */
    bool ei_delay;
    /* T-state counter for current instruction */
    int  t_states;
    /* Total T-states executed */
    u64  total_tstates;

    /* Memory and I/O callbacks */
    u8   (*mem_read)(void *ctx, u16 addr);
    void (*mem_write)(void *ctx, u16 addr, u8 data);
    u8   (*port_read)(void *ctx, u16 port);
    void (*port_write)(void *ctx, u16 port, u8 data);
    /* Opcode fetch (for M1 cycle detection — accelerator, DOS traps) */
    u8   (*opcode_fetch)(void *ctx, u16 addr);
    void *callback_ctx;

    /* Accelerator hook: called on LD r,r where r==r */
    /* Returns extra T-states consumed, 0 if not handled */
    int  (*accel_hook)(void *ctx, u8 opcode);
    void *accel_ctx;
} z80_t;

/* Convenience register access macros */
#define Z80_A   cpu->af.hi
#define Z80_F   cpu->af.lo
#define Z80_B   cpu->bc.hi
#define Z80_C   cpu->bc.lo
#define Z80_D   cpu->de.hi
#define Z80_E   cpu->de.lo
#define Z80_H   cpu->hl.hi
#define Z80_L   cpu->hl.lo
#define Z80_AF  cpu->af.w
#define Z80_BC  cpu->bc.w
#define Z80_DE  cpu->de.w
#define Z80_HL  cpu->hl.w
#define Z80_IX  cpu->ix.w
#define Z80_IY  cpu->iy.w
#define Z80_SP  cpu->sp.w
#define Z80_PC  cpu->pc.w
#define Z80_WZ  cpu->wz.w
#define Z80_IXH cpu->ix.hi
#define Z80_IXL cpu->ix.lo
#define Z80_IYH cpu->iy.hi
#define Z80_IYL cpu->iy.lo

/* Flag bits */
#define Z80_FLAG_C   0x01   /* Carry */
#define Z80_FLAG_N   0x02   /* Subtract */
#define Z80_FLAG_PV  0x04   /* Parity/Overflow */
#define Z80_FLAG_3   0x08   /* Undocumented bit 3 */
#define Z80_FLAG_H   0x10   /* Half carry */
#define Z80_FLAG_5   0x20   /* Undocumented bit 5 */
#define Z80_FLAG_Z   0x40   /* Zero */
#define Z80_FLAG_S   0x80   /* Sign */

/* Initialize CPU to power-on state */
void z80_init(z80_t *cpu);

/* Reset CPU (as if /RESET pin asserted) */
void z80_reset(z80_t *cpu);

/* Execute one instruction. Returns T-states consumed. */
int z80_step(z80_t *cpu);

/* Execute instructions for at least `tstates` T-states. Returns actual. */
int z80_execute(z80_t *cpu, int tstates);

/* Signal maskable interrupt (active low) */
void z80_irq(z80_t *cpu, u8 vector);

/* Signal non-maskable interrupt */
void z80_nmi(z80_t *cpu);

/* Opcode execution — called from z80_step, defined in z80_ops.c */
int z80_exec_op(z80_t *cpu, u8 opcode);
int z80_exec_cb(z80_t *cpu);
int z80_exec_ed(z80_t *cpu);
int z80_exec_dd(z80_t *cpu);  /* IX prefix */
int z80_exec_fd(z80_t *cpu);  /* IY prefix */
int z80_exec_ddcb(z80_t *cpu, i8 displacement);
int z80_exec_fdcb(z80_t *cpu, i8 displacement);

/* Flag/parity tables — defined in z80_tables.c */
extern u8 z80_sz53_table[256];     /* Sign, Zero, bits 5+3 */
extern u8 z80_parity_table[256];   /* Parity flag */
extern u8 z80_sz53p_table[256];    /* Sign, Zero, bits 5+3, Parity */

void z80_init_tables(void);

#endif /* SPEMU_Z80_H */
