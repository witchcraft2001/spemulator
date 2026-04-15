/*
 * SPEmulator — Z80 DDCB/FDCB prefixed opcodes
 * Bit operations on (IX+d)/(IY+d) with optional copy to register.
 */
#include "z80.h"

static inline u8 rd(z80_t *cpu, u16 addr) { return cpu->mem_read(cpu->callback_ctx, addr); }
static inline void wr(z80_t *cpu, u16 addr, u8 data) { cpu->mem_write(cpu->callback_ctx, addr, data); }

/* Common DDCB/FDCB implementation. addr = IX/IY + d (already in WZ). */
static int exec_xxcb(z80_t *cpu, u16 addr) {
    u8 opcode = rd(cpu, Z80_PC);
    Z80_PC++;

    u8 val = rd(cpu, addr);
    u8 result = val;
    int r = opcode & 7;  /* destination register (undocumented for non-BIT) */

    switch (opcode & 0xF8) {
    /* Rotate/shift operations */
    case 0x00: { u8 c = val >> 7; result = (val << 1) | c; Z80_F = z80_sz53p_table[result] | c; break; } /* RLC */
    case 0x08: { u8 c = val & 1; result = (val >> 1) | (c << 7); Z80_F = z80_sz53p_table[result] | c; break; } /* RRC */
    case 0x10: { u8 c = val >> 7; result = (val << 1) | (Z80_F & Z80_FLAG_C); Z80_F = z80_sz53p_table[result] | c; break; } /* RL */
    case 0x18: { u8 c = val & 1; result = (val >> 1) | ((Z80_F & Z80_FLAG_C) << 7); Z80_F = z80_sz53p_table[result] | c; break; } /* RR */
    case 0x20: { u8 c = val >> 7; result = val << 1; Z80_F = z80_sz53p_table[result] | c; break; } /* SLA */
    case 0x28: { u8 c = val & 1; result = (val >> 1) | (val & 0x80); Z80_F = z80_sz53p_table[result] | c; break; } /* SRA */
    case 0x30: { u8 c = val >> 7; result = (val << 1) | 1; Z80_F = z80_sz53p_table[result] | c; break; } /* SLL (undoc) */
    case 0x38: { u8 c = val & 1; result = val >> 1; Z80_F = z80_sz53p_table[result] | c; break; } /* SRL */

    /* BIT b,(IX+d) — does NOT store result */
    case 0x40: case 0x48: case 0x50: case 0x58:
    case 0x60: case 0x68: case 0x70: case 0x78: {
        int bit = (opcode >> 3) & 7;
        u8 bval = val & (1 << bit);
        Z80_F = (Z80_F & Z80_FLAG_C) | Z80_FLAG_H
              | (bval ? 0 : (Z80_FLAG_Z | Z80_FLAG_PV))
              | (bval & Z80_FLAG_S)
              | (cpu->wz.hi & (Z80_FLAG_5 | Z80_FLAG_3)); /* undoc: bits 5,3 from WZ high */
        return 20;
    }

    /* RES b,(IX+d) */
    case 0x80: case 0x88: case 0x90: case 0x98:
    case 0xA0: case 0xA8: case 0xB0: case 0xB8:
        result = val & ~(1 << ((opcode >> 3) & 7));
        break;

    /* SET b,(IX+d) */
    case 0xC0: case 0xC8: case 0xD0: case 0xD8:
    case 0xE0: case 0xE8: case 0xF0: case 0xF8:
        result = val | (1 << ((opcode >> 3) & 7));
        break;
    }

    /* Write result back to memory */
    wr(cpu, addr, result);

    /* Undocumented: also copy to register if r != 6 */
    if (r != 6) {
        switch (r) {
        case 0: Z80_B = result; break;
        case 1: Z80_C = result; break;
        case 2: Z80_D = result; break;
        case 3: Z80_E = result; break;
        case 4: Z80_H = result; break;
        case 5: Z80_L = result; break;
        case 7: Z80_A = result; break;
        }
    }

    return 23;
}

int z80_exec_ddcb(z80_t *cpu, i8 displacement) {
    u16 addr = Z80_IX + displacement;
    Z80_WZ = addr;
    return exec_xxcb(cpu, addr);
}

int z80_exec_fdcb(z80_t *cpu, i8 displacement) {
    u16 addr = Z80_IY + displacement;
    Z80_WZ = addr;
    return exec_xxcb(cpu, addr);
}
