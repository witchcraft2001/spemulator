/*
 * SPEmulator — Z80 CPU Core
 * Initialization, reset, step/execute, interrupts.
 */
#include "z80.h"
#include <string.h>

/* Helper: read byte from memory */
static inline u8 rd(z80_t *cpu, u16 addr) {
    return cpu->mem_read(cpu->callback_ctx, addr);
}

/* Helper: write byte to memory */
static inline void wr(z80_t *cpu, u16 addr, u8 data) {
    cpu->mem_write(cpu->callback_ctx, addr, data);
}

/* Helper: fetch opcode at PC (M1 cycle) */
static inline u8 fetch(z80_t *cpu) {
    u8 op;
    if (cpu->opcode_fetch)
        op = cpu->opcode_fetch(cpu->callback_ctx, Z80_PC);
    else
        op = rd(cpu, Z80_PC);
    Z80_PC++;
    cpu->r = (cpu->r & 0x80) | ((cpu->r + 1) & 0x7f);
    return op;
}

void z80_init(z80_t *cpu) {
    memset(cpu, 0, sizeof(z80_t));
    z80_init_tables();
    z80_reset(cpu);
}

void z80_reset(z80_t *cpu) {
    Z80_AF = 0xFFFF;
    Z80_SP = 0xFFFF;
    Z80_PC = 0x0000;
    Z80_BC = 0x0000;
    Z80_DE = 0x0000;
    Z80_HL = 0x0000;
    Z80_IX = 0x0000;
    Z80_IY = 0x0000;
    Z80_WZ = 0x0000;
    cpu->af2.w = 0x0000;
    cpu->bc2.w = 0x0000;
    cpu->de2.w = 0x0000;
    cpu->hl2.w = 0x0000;
    cpu->i = 0;
    cpu->r = 0;
    cpu->r7 = 0;
    cpu->iff1 = 0;
    cpu->iff2 = 0;
    cpu->im = 0;
    cpu->halted = false;
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->ei_delay = false;
    cpu->t_states = 0;
}

int z80_step(z80_t *cpu) {
    cpu->t_states = 0;

    /* Check for NMI */
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        cpu->halted = false;
        cpu->iff2 = cpu->iff1;
        cpu->iff1 = 0;
        /* Push PC */
        Z80_SP--;
        wr(cpu, Z80_SP, (u8)(Z80_PC >> 8));
        Z80_SP--;
        wr(cpu, Z80_SP, (u8)(Z80_PC & 0xFF));
        Z80_PC = 0x0066;
        Z80_WZ = 0x0066;
        cpu->t_states += 11;
        return cpu->t_states;
    }

    /* Check for IRQ */
    if (cpu->irq_pending && cpu->iff1 && !cpu->ei_delay) {
        cpu->halted = false;
        cpu->iff1 = 0;
        cpu->iff2 = 0;
        cpu->irq_pending = false;

        switch (cpu->im) {
        case 0:
            /* Execute instruction on data bus (usually RST 38h) */
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC >> 8));
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC & 0xFF));
            Z80_PC = (u16)(cpu->irq_vector & 0x38);
            Z80_WZ = Z80_PC;
            cpu->t_states += 13;
            return cpu->t_states;

        case 1:
            /* RST 38h */
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC >> 8));
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC & 0xFF));
            Z80_PC = 0x0038;
            Z80_WZ = 0x0038;
            cpu->t_states += 13;
            return cpu->t_states;

        case 2: {
            /* Vectored: address = (I << 8) | vector */
            u16 vec_addr = ((u16)cpu->i << 8) | (cpu->irq_vector & 0xFE);
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC >> 8));
            Z80_SP--;
            wr(cpu, Z80_SP, (u8)(Z80_PC & 0xFF));
            u8 lo = rd(cpu, vec_addr);
            u8 hi = rd(cpu, vec_addr + 1);
            Z80_PC = (u16)(lo | (hi << 8));
            Z80_WZ = Z80_PC;
            cpu->t_states += 19;
            return cpu->t_states;
        }
        }
    }

    cpu->ei_delay = false;

    /* If halted, just consume 4 T-states (NOP equivalent) */
    if (cpu->halted) {
        cpu->r = (cpu->r & 0x80) | ((cpu->r + 1) & 0x7f);
        cpu->t_states += 4;
        return cpu->t_states;
    }

    /* Fetch and execute opcode */
    u8 opcode = fetch(cpu);
    cpu->t_states += z80_exec_op(cpu, opcode);

    return cpu->t_states;
}

int z80_execute(z80_t *cpu, int tstates) {
    int executed = 0;
    while (executed < tstates) {
        int t = z80_step(cpu);
        executed += t;
        cpu->total_tstates += t;
    }
    return executed;
}

void z80_irq(z80_t *cpu, u8 vector) {
    cpu->irq_pending = true;
    cpu->irq_vector = vector;
}

void z80_nmi(z80_t *cpu) {
    cpu->nmi_pending = true;
}
