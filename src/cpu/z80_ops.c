/*
 * SPEmulator — Z80 Main Opcode Table
 * Implements all unprefixed Z80 opcodes with correct T-state timing.
 */
#include "z80.h"

/* --- Helpers (inline for performance) --- */

static inline u8 rd(z80_t *cpu, u16 addr) {
    return cpu->mem_read(cpu->callback_ctx, addr);
}
static inline void wr(z80_t *cpu, u16 addr, u8 data) {
    cpu->mem_write(cpu->callback_ctx, addr, data);
}
static inline u8 fetch_byte(z80_t *cpu) {
    u8 v = rd(cpu, Z80_PC);
    Z80_PC++;
    return v;
}
static inline u16 fetch_word(z80_t *cpu) {
    u8 lo = fetch_byte(cpu);
    u8 hi = fetch_byte(cpu);
    return (u16)(lo | (hi << 8));
}
static inline void push16(z80_t *cpu, u16 val) {
    Z80_SP--;
    wr(cpu, Z80_SP, (u8)(val >> 8));
    Z80_SP--;
    wr(cpu, Z80_SP, (u8)(val & 0xFF));
}
static inline u16 pop16(z80_t *cpu) {
    u8 lo = rd(cpu, Z80_SP); Z80_SP++;
    u8 hi = rd(cpu, Z80_SP); Z80_SP++;
    return (u16)(lo | (hi << 8));
}

/* --- ALU operations --- */

static inline void alu_add_a(z80_t *cpu, u8 val) {
    u16 result = (u16)Z80_A + val;
    u8 lookup = ((Z80_A & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    Z80_A = (u8)result;
    Z80_F = ((result & 0x100) ? Z80_FLAG_C : 0)
          | z80_sz53_table[Z80_A]
          | ((lookup & 0x07) ? Z80_FLAG_H : 0)  /* half-carry from lookup */
          | (((lookup >> 4) ^ (lookup >> 5)) & Z80_FLAG_PV); /* overflow */
    /* More precise half-carry */
    Z80_F &= ~Z80_FLAG_H;
    if (((Z80_A ^ val ^ (u8)result) & 0x10)) Z80_F |= Z80_FLAG_H;
    /* More precise overflow */
    Z80_F &= ~Z80_FLAG_PV;
    if (((val ^ Z80_A ^ 0x80) & (val ^ (u8)result) & 0x80)) /* removed, use below */ {}
    /* Recompute cleanly */
    u8 r8 = (u8)result;
    Z80_F = ((result & 0x100) ? Z80_FLAG_C : 0)
          | z80_sz53_table[r8]
          | (((Z80_A ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0);
    /* Overflow: operands same sign, result different sign (but this is ADD so both positive-sense) */
    /* For ADD: overflow if (A_old ^ val) bit7 == 0 AND (A_old ^ result) bit7 == 1 */
    /* We need A before the add, but we already overwrote it. Let's restructure. */
    /* Actually, let me redo this properly: */
    (void)r8;
    /* This function is broken, let me rewrite below */
}

/* Let me rewrite ALU operations cleanly */

static inline void z80_add_a(z80_t *cpu, u8 val) {
    u8 a = Z80_A;
    u16 result = (u16)a + val;
    u8 r8 = (u8)result;
    Z80_F = ((result >> 8) & Z80_FLAG_C)
          | z80_sz53_table[r8]
          | (((a ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0)
          | ((((a ^ val) ^ 0x80) & (a ^ r8) & 0x80) ? Z80_FLAG_PV : 0);
    Z80_A = r8;
}

static inline void z80_adc_a(z80_t *cpu, u8 val) {
    u8 a = Z80_A;
    u8 c = Z80_F & Z80_FLAG_C;
    u16 result = (u16)a + val + c;
    u8 r8 = (u8)result;
    Z80_F = ((result >> 8) & Z80_FLAG_C)
          | z80_sz53_table[r8]
          | (((a ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0)
          | ((((a ^ val) ^ 0x80) & (a ^ r8) & 0x80) ? Z80_FLAG_PV : 0);
    Z80_A = r8;
}

static inline void z80_sub_a(z80_t *cpu, u8 val) {
    u8 a = Z80_A;
    u16 result = (u16)a - val;
    u8 r8 = (u8)result;
    Z80_F = ((result >> 8) & Z80_FLAG_C)
          | Z80_FLAG_N
          | z80_sz53_table[r8]
          | (((a ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0)
          | (((a ^ val) & (a ^ r8) & 0x80) ? Z80_FLAG_PV : 0);
    Z80_A = r8;
}

static inline void z80_sbc_a(z80_t *cpu, u8 val) {
    u8 a = Z80_A;
    u8 c = Z80_F & Z80_FLAG_C;
    u16 result = (u16)a - val - c;
    u8 r8 = (u8)result;
    Z80_F = ((result >> 8) & Z80_FLAG_C)
          | Z80_FLAG_N
          | z80_sz53_table[r8]
          | (((a ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0)
          | (((a ^ val) & (a ^ r8) & 0x80) ? Z80_FLAG_PV : 0);
    Z80_A = r8;
}

static inline void z80_and_a(z80_t *cpu, u8 val) {
    Z80_A &= val;
    Z80_F = z80_sz53p_table[Z80_A] | Z80_FLAG_H;
}

static inline void z80_xor_a(z80_t *cpu, u8 val) {
    Z80_A ^= val;
    Z80_F = z80_sz53p_table[Z80_A];
}

static inline void z80_or_a(z80_t *cpu, u8 val) {
    Z80_A |= val;
    Z80_F = z80_sz53p_table[Z80_A];
}

static inline void z80_cp_a(z80_t *cpu, u8 val) {
    u8 a = Z80_A;
    u16 result = (u16)a - val;
    u8 r8 = (u8)result;
    Z80_F = ((result >> 8) & Z80_FLAG_C)
          | Z80_FLAG_N
          | (r8 & Z80_FLAG_S)
          | ((r8 == 0) ? Z80_FLAG_Z : 0)
          | (val & (Z80_FLAG_5 | Z80_FLAG_3))  /* bits 5,3 from operand, not result */
          | (((a ^ val ^ r8) & 0x10) ? Z80_FLAG_H : 0)
          | (((a ^ val) & (a ^ r8) & 0x80) ? Z80_FLAG_PV : 0);
}

static inline void z80_inc(z80_t *cpu, u8 *reg) {
    (*reg)++;
    Z80_F = (Z80_F & Z80_FLAG_C)
          | z80_sz53_table[*reg]
          | ((*reg == 0x80) ? Z80_FLAG_PV : 0)
          | ((*reg & 0x0F) ? 0 : Z80_FLAG_H);
}

static inline void z80_dec(z80_t *cpu, u8 *reg) {
    Z80_F = (Z80_F & Z80_FLAG_C)
          | (((*reg & 0x0F) == 0) ? Z80_FLAG_H : 0)
          | ((*reg == 0x80) ? Z80_FLAG_PV : 0);
    (*reg)--;
    Z80_F |= Z80_FLAG_N | z80_sz53_table[*reg];
}

static inline void z80_add_hl(z80_t *cpu, u16 val) {
    u32 result = (u32)Z80_HL + val;
    Z80_WZ = Z80_HL + 1;
    Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
          | ((result >> 16) & Z80_FLAG_C)
          | ((result >> 8) & (Z80_FLAG_5 | Z80_FLAG_3))
          | (((Z80_HL ^ val ^ (u16)result) & 0x1000) ? Z80_FLAG_H : 0);
    Z80_HL = (u16)result;
}

/* 8-bit register decode for opcodes */
static inline u8 z80_get_reg8(z80_t *cpu, int idx) {
    switch (idx) {
    case 0: return Z80_B;
    case 1: return Z80_C;
    case 2: return Z80_D;
    case 3: return Z80_E;
    case 4: return Z80_H;
    case 5: return Z80_L;
    case 6: return rd(cpu, Z80_HL);  /* (HL) */
    case 7: return Z80_A;
    }
    return 0;
}

static inline void z80_set_reg8(z80_t *cpu, int idx, u8 val) {
    switch (idx) {
    case 0: Z80_B = val; break;
    case 1: Z80_C = val; break;
    case 2: Z80_D = val; break;
    case 3: Z80_E = val; break;
    case 4: Z80_H = val; break;
    case 5: Z80_L = val; break;
    case 6: wr(cpu, Z80_HL, val); break;
    case 7: Z80_A = val; break;
    }
}

static inline u16 *z80_get_reg16(z80_t *cpu, int idx) {
    switch (idx) {
    case 0: return &Z80_BC;
    case 1: return &Z80_DE;
    case 2: return &Z80_HL;
    case 3: return &Z80_SP;
    }
    return &Z80_HL;
}

static inline u16 *z80_get_reg16_af(z80_t *cpu, int idx) {
    switch (idx) {
    case 0: return &Z80_BC;
    case 1: return &Z80_DE;
    case 2: return &Z80_HL;
    case 3: return &Z80_AF;
    }
    return &Z80_AF;
}

static inline bool z80_check_cc(z80_t *cpu, int cc) {
    switch (cc) {
    case 0: return !(Z80_F & Z80_FLAG_Z);   /* NZ */
    case 1: return  (Z80_F & Z80_FLAG_Z);   /* Z */
    case 2: return !(Z80_F & Z80_FLAG_C);   /* NC */
    case 3: return  (Z80_F & Z80_FLAG_C);   /* C */
    case 4: return !(Z80_F & Z80_FLAG_PV);  /* PO */
    case 5: return  (Z80_F & Z80_FLAG_PV);  /* PE */
    case 6: return !(Z80_F & Z80_FLAG_S);   /* P */
    case 7: return  (Z80_F & Z80_FLAG_S);   /* M */
    }
    return false;
}

/* Main opcode executor. Returns T-states (not counting the 4 for fetch). */
int z80_exec_op(z80_t *cpu, u8 opcode) {
    int ts = 0;

    switch (opcode) {
    /* === NOP === */
    case 0x00: /* NOP */
        return 4;

    /* === LD rr,nn === */
    case 0x01: Z80_BC = fetch_word(cpu); return 10;
    case 0x11: Z80_DE = fetch_word(cpu); return 10;
    case 0x21: Z80_HL = fetch_word(cpu); return 10;
    case 0x31: Z80_SP = fetch_word(cpu); return 10;

    /* === LD (BC/DE),A / LD A,(BC/DE) === */
    case 0x02: wr(cpu, Z80_BC, Z80_A); Z80_WZ = ((Z80_A << 8) | ((Z80_BC + 1) & 0xFF)); return 7;
    case 0x12: wr(cpu, Z80_DE, Z80_A); Z80_WZ = ((Z80_A << 8) | ((Z80_DE + 1) & 0xFF)); return 7;
    case 0x0A: Z80_A = rd(cpu, Z80_BC); Z80_WZ = Z80_BC + 1; return 7;
    case 0x1A: Z80_A = rd(cpu, Z80_DE); Z80_WZ = Z80_DE + 1; return 7;

    /* === LD (nn),HL / LD HL,(nn) === */
    case 0x22: { u16 addr = fetch_word(cpu); wr(cpu, addr, Z80_L); wr(cpu, addr+1, Z80_H); Z80_WZ = addr+1; return 16; }
    case 0x2A: { u16 addr = fetch_word(cpu); Z80_L = rd(cpu, addr); Z80_H = rd(cpu, addr+1); Z80_WZ = addr+1; return 16; }

    /* === LD (nn),A / LD A,(nn) === */
    case 0x32: { u16 addr = fetch_word(cpu); wr(cpu, addr, Z80_A); Z80_WZ = ((Z80_A << 8) | ((addr + 1) & 0xFF)); return 13; }
    case 0x3A: { u16 addr = fetch_word(cpu); Z80_A = rd(cpu, addr); Z80_WZ = addr + 1; return 13; }

    /* === INC/DEC rr === */
    case 0x03: Z80_BC++; return 6;
    case 0x13: Z80_DE++; return 6;
    case 0x23: Z80_HL++; return 6;
    case 0x33: Z80_SP++; return 6;
    case 0x0B: Z80_BC--; return 6;
    case 0x1B: Z80_DE--; return 6;
    case 0x2B: Z80_HL--; return 6;
    case 0x3B: Z80_SP--; return 6;

    /* === INC r === */
    case 0x04: z80_inc(cpu, &Z80_B); return 4;
    case 0x0C: z80_inc(cpu, &Z80_C); return 4;
    case 0x14: z80_inc(cpu, &Z80_D); return 4;
    case 0x1C: z80_inc(cpu, &Z80_E); return 4;
    case 0x24: z80_inc(cpu, &Z80_H); return 4;
    case 0x2C: z80_inc(cpu, &Z80_L); return 4;
    case 0x34: { u8 v = rd(cpu, Z80_HL); z80_inc(cpu, &v); wr(cpu, Z80_HL, v); return 11; }
    case 0x3C: z80_inc(cpu, &Z80_A); return 4;

    /* === DEC r === */
    case 0x05: z80_dec(cpu, &Z80_B); return 4;
    case 0x0D: z80_dec(cpu, &Z80_C); return 4;
    case 0x15: z80_dec(cpu, &Z80_D); return 4;
    case 0x1D: z80_dec(cpu, &Z80_E); return 4;
    case 0x25: z80_dec(cpu, &Z80_H); return 4;
    case 0x2D: z80_dec(cpu, &Z80_L); return 4;
    case 0x35: { u8 v = rd(cpu, Z80_HL); z80_dec(cpu, &v); wr(cpu, Z80_HL, v); return 11; }
    case 0x3D: z80_dec(cpu, &Z80_A); return 4;

    /* === LD r,n === */
    case 0x06: Z80_B = fetch_byte(cpu); return 7;
    case 0x0E: Z80_C = fetch_byte(cpu); return 7;
    case 0x16: Z80_D = fetch_byte(cpu); return 7;
    case 0x1E: Z80_E = fetch_byte(cpu); return 7;
    case 0x26: Z80_H = fetch_byte(cpu); return 7;
    case 0x2E: Z80_L = fetch_byte(cpu); return 7;
    case 0x36: wr(cpu, Z80_HL, fetch_byte(cpu)); return 10;
    case 0x3E: Z80_A = fetch_byte(cpu); return 7;

    /* === RLCA, RRCA, RLA, RRA === */
    case 0x07: { /* RLCA */
        u8 a = Z80_A;
        Z80_A = (a << 1) | (a >> 7);
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3))
              | (a >> 7);  /* carry = old bit 7 */
        return 4;
    }
    case 0x0F: { /* RRCA */
        u8 a = Z80_A;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | (a & Z80_FLAG_C);
        Z80_A = (a >> 1) | (a << 7);
        Z80_F |= Z80_A & (Z80_FLAG_5 | Z80_FLAG_3);
        return 4;
    }
    case 0x17: { /* RLA */
        u8 a = Z80_A;
        Z80_A = (a << 1) | (Z80_F & Z80_FLAG_C);
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3))
              | (a >> 7);
        return 4;
    }
    case 0x1F: { /* RRA */
        u8 a = Z80_A;
        Z80_A = (a >> 1) | ((Z80_F & Z80_FLAG_C) << 7);
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3))
              | (a & Z80_FLAG_C);
        return 4;
    }

    /* === EX AF,AF' === */
    case 0x08: { u16 tmp = Z80_AF; Z80_AF = cpu->af2.w; cpu->af2.w = tmp; return 4; }

    /* === ADD HL,rr === */
    case 0x09: z80_add_hl(cpu, Z80_BC); return 11;
    case 0x19: z80_add_hl(cpu, Z80_DE); return 11;
    case 0x29: z80_add_hl(cpu, Z80_HL); return 11;
    case 0x39: z80_add_hl(cpu, Z80_SP); return 11;

    /* === DAA === */
    case 0x27: {
        u8 a = Z80_A;
        u8 correction = 0;
        u8 carry = Z80_F & Z80_FLAG_C;
        if ((Z80_F & Z80_FLAG_H) || ((a & 0x0F) > 9)) correction |= 0x06;
        if (carry || (a > 0x99)) { correction |= 0x60; carry = Z80_FLAG_C; }
        if (Z80_F & Z80_FLAG_N)
            Z80_A -= correction;
        else
            Z80_A += correction;
        Z80_F = z80_sz53p_table[Z80_A]
              | carry
              | (Z80_F & Z80_FLAG_N)
              | ((a ^ Z80_A) & Z80_FLAG_H);
        return 4;
    }

    /* === CPL === */
    case 0x2F:
        Z80_A ^= 0xFF;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV | Z80_FLAG_C))
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3))
              | Z80_FLAG_H | Z80_FLAG_N;
        return 4;

    /* === SCF === */
    case 0x37:
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3))
              | Z80_FLAG_C;
        return 4;

    /* === CCF === */
    case 0x3F:
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | ((Z80_F & Z80_FLAG_C) ? Z80_FLAG_H : Z80_FLAG_C)
              | (Z80_A & (Z80_FLAG_5 | Z80_FLAG_3));
        /* Undocumented: H = old C */
        return 4;

    /* === JR e === */
    case 0x18: { i8 e = (i8)fetch_byte(cpu); Z80_PC += e; Z80_WZ = Z80_PC; return 12; }

    /* === DJNZ e === */
    case 0x10: {
        i8 e = (i8)fetch_byte(cpu);
        Z80_B--;
        if (Z80_B) { Z80_PC += e; Z80_WZ = Z80_PC; return 13; }
        return 8;
    }

    /* === JR cc,e === */
    case 0x20: { i8 e = (i8)fetch_byte(cpu); if (!(Z80_F & Z80_FLAG_Z)) { Z80_PC += e; Z80_WZ = Z80_PC; return 12; } return 7; }
    case 0x28: { i8 e = (i8)fetch_byte(cpu); if ( (Z80_F & Z80_FLAG_Z)) { Z80_PC += e; Z80_WZ = Z80_PC; return 12; } return 7; }
    case 0x30: { i8 e = (i8)fetch_byte(cpu); if (!(Z80_F & Z80_FLAG_C)) { Z80_PC += e; Z80_WZ = Z80_PC; return 12; } return 7; }
    case 0x38: { i8 e = (i8)fetch_byte(cpu); if ( (Z80_F & Z80_FLAG_C)) { Z80_PC += e; Z80_WZ = Z80_PC; return 12; } return 7; }

    /* === LD r,r' === (0x40-0x7F except 0x76=HALT) */
    /* These include the accelerator-intercepted LD r,r where src==dst */
    case 0x76: /* HALT */
        cpu->halted = true;
        Z80_PC--;  /* re-execute HALT until interrupted */
        return 4;

    /* LD B,r */
    case 0x40: /* LD B,B — accelerator: disable */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x40); if (ts) return 4 + ts; }
        return 4;
    case 0x41: Z80_B = Z80_C; return 4;
    case 0x42: Z80_B = Z80_D; return 4;
    case 0x43: Z80_B = Z80_E; return 4;
    case 0x44: Z80_B = Z80_H; return 4;
    case 0x45: Z80_B = Z80_L; return 4;
    case 0x46: Z80_B = rd(cpu, Z80_HL); return 7;
    case 0x47: Z80_B = Z80_A; return 4;

    /* LD C,r */
    case 0x48: Z80_C = Z80_B; return 4;
    case 0x49: /* LD C,C — accelerator: fill block */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x49); if (ts) return 4 + ts; }
        return 4;
    case 0x4A: Z80_C = Z80_D; return 4;
    case 0x4B: Z80_C = Z80_E; return 4;
    case 0x4C: Z80_C = Z80_H; return 4;
    case 0x4D: Z80_C = Z80_L; return 4;
    case 0x4E: Z80_C = rd(cpu, Z80_HL); return 7;
    case 0x4F: Z80_C = Z80_A; return 4;

    /* LD D,r */
    case 0x50: Z80_D = Z80_B; return 4;
    case 0x51: Z80_D = Z80_C; return 4;
    case 0x52: /* LD D,D — accelerator: set block size */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x52); if (ts) return 4 + ts; }
        return 4;
    case 0x53: Z80_D = Z80_E; return 4;
    case 0x54: Z80_D = Z80_H; return 4;
    case 0x55: Z80_D = Z80_L; return 4;
    case 0x56: Z80_D = rd(cpu, Z80_HL); return 7;
    case 0x57: Z80_D = Z80_A; return 4;

    /* LD E,r */
    case 0x58: Z80_E = Z80_B; return 4;
    case 0x59: Z80_E = Z80_C; return 4;
    case 0x5A: Z80_E = Z80_D; return 4;
    case 0x5B: /* LD E,E — accelerator: vertical fill */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x5B); if (ts) return 4 + ts; }
        return 4;
    case 0x5C: Z80_E = Z80_H; return 4;
    case 0x5D: Z80_E = Z80_L; return 4;
    case 0x5E: Z80_E = rd(cpu, Z80_HL); return 7;
    case 0x5F: Z80_E = Z80_A; return 4;

    /* LD H,r */
    case 0x60: Z80_H = Z80_B; return 4;
    case 0x61: Z80_H = Z80_C; return 4;
    case 0x62: Z80_H = Z80_D; return 4;
    case 0x63: Z80_H = Z80_E; return 4;
    case 0x64: /* LD H,H */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x64); if (ts) return 4 + ts; }
        return 4;
    case 0x65: Z80_H = Z80_L; return 4;
    case 0x66: Z80_H = rd(cpu, Z80_HL); return 7;
    case 0x67: Z80_H = Z80_A; return 4;

    /* LD L,r */
    case 0x68: Z80_L = Z80_B; return 4;
    case 0x69: Z80_L = Z80_C; return 4;
    case 0x6A: Z80_L = Z80_D; return 4;
    case 0x6B: Z80_L = Z80_E; return 4;
    case 0x6C: Z80_L = Z80_H; return 4;
    case 0x6D: /* LD L,L — accelerator: copy row */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x6D); if (ts) return 4 + ts; }
        return 4;
    case 0x6E: Z80_L = rd(cpu, Z80_HL); return 7;
    case 0x6F: Z80_L = Z80_A; return 4;

    /* LD (HL),r */
    case 0x70: wr(cpu, Z80_HL, Z80_B); return 7;
    case 0x71: wr(cpu, Z80_HL, Z80_C); return 7;
    case 0x72: wr(cpu, Z80_HL, Z80_D); return 7;
    case 0x73: wr(cpu, Z80_HL, Z80_E); return 7;
    case 0x74: wr(cpu, Z80_HL, Z80_H); return 7;
    case 0x75: wr(cpu, Z80_HL, Z80_L); return 7;
    /* 0x76 = HALT, handled above */
    case 0x77: wr(cpu, Z80_HL, Z80_A); return 7;

    /* LD A,r */
    case 0x78: Z80_A = Z80_B; return 4;
    case 0x79: Z80_A = Z80_C; return 4;
    case 0x7A: Z80_A = Z80_D; return 4;
    case 0x7B: Z80_A = Z80_E; return 4;
    case 0x7C: Z80_A = Z80_H; return 4;
    case 0x7D: Z80_A = Z80_L; return 4;
    case 0x7E: Z80_A = rd(cpu, Z80_HL); return 7;
    case 0x7F: /* LD A,A — accelerator: copy vertical */
        if (cpu->accel_hook) { ts = cpu->accel_hook(cpu->accel_ctx, 0x7F); if (ts) return 4 + ts; }
        return 4;

    /* === ALU A,r (0x80-0xBF) === */
    case 0x80: z80_add_a(cpu, Z80_B); return 4;
    case 0x81: z80_add_a(cpu, Z80_C); return 4;
    case 0x82: z80_add_a(cpu, Z80_D); return 4;
    case 0x83: z80_add_a(cpu, Z80_E); return 4;
    case 0x84: z80_add_a(cpu, Z80_H); return 4;
    case 0x85: z80_add_a(cpu, Z80_L); return 4;
    case 0x86: z80_add_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0x87: z80_add_a(cpu, Z80_A); return 4;

    case 0x88: z80_adc_a(cpu, Z80_B); return 4;
    case 0x89: z80_adc_a(cpu, Z80_C); return 4;
    case 0x8A: z80_adc_a(cpu, Z80_D); return 4;
    case 0x8B: z80_adc_a(cpu, Z80_E); return 4;
    case 0x8C: z80_adc_a(cpu, Z80_H); return 4;
    case 0x8D: z80_adc_a(cpu, Z80_L); return 4;
    case 0x8E: z80_adc_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0x8F: z80_adc_a(cpu, Z80_A); return 4;

    case 0x90: z80_sub_a(cpu, Z80_B); return 4;
    case 0x91: z80_sub_a(cpu, Z80_C); return 4;
    case 0x92: z80_sub_a(cpu, Z80_D); return 4;
    case 0x93: z80_sub_a(cpu, Z80_E); return 4;
    case 0x94: z80_sub_a(cpu, Z80_H); return 4;
    case 0x95: z80_sub_a(cpu, Z80_L); return 4;
    case 0x96: z80_sub_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0x97: z80_sub_a(cpu, Z80_A); return 4;

    case 0x98: z80_sbc_a(cpu, Z80_B); return 4;
    case 0x99: z80_sbc_a(cpu, Z80_C); return 4;
    case 0x9A: z80_sbc_a(cpu, Z80_D); return 4;
    case 0x9B: z80_sbc_a(cpu, Z80_E); return 4;
    case 0x9C: z80_sbc_a(cpu, Z80_H); return 4;
    case 0x9D: z80_sbc_a(cpu, Z80_L); return 4;
    case 0x9E: z80_sbc_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0x9F: z80_sbc_a(cpu, Z80_A); return 4;

    case 0xA0: z80_and_a(cpu, Z80_B); return 4;
    case 0xA1: z80_and_a(cpu, Z80_C); return 4;
    case 0xA2: z80_and_a(cpu, Z80_D); return 4;
    case 0xA3: z80_and_a(cpu, Z80_E); return 4;
    case 0xA4: z80_and_a(cpu, Z80_H); return 4;
    case 0xA5: z80_and_a(cpu, Z80_L); return 4;
    case 0xA6: z80_and_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0xA7: z80_and_a(cpu, Z80_A); return 4;

    case 0xA8: z80_xor_a(cpu, Z80_B); return 4;
    case 0xA9: z80_xor_a(cpu, Z80_C); return 4;
    case 0xAA: z80_xor_a(cpu, Z80_D); return 4;
    case 0xAB: z80_xor_a(cpu, Z80_E); return 4;
    case 0xAC: z80_xor_a(cpu, Z80_H); return 4;
    case 0xAD: z80_xor_a(cpu, Z80_L); return 4;
    case 0xAE: z80_xor_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0xAF: z80_xor_a(cpu, Z80_A); return 4;

    case 0xB0: z80_or_a(cpu, Z80_B); return 4;
    case 0xB1: z80_or_a(cpu, Z80_C); return 4;
    case 0xB2: z80_or_a(cpu, Z80_D); return 4;
    case 0xB3: z80_or_a(cpu, Z80_E); return 4;
    case 0xB4: z80_or_a(cpu, Z80_H); return 4;
    case 0xB5: z80_or_a(cpu, Z80_L); return 4;
    case 0xB6: z80_or_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0xB7: z80_or_a(cpu, Z80_A); return 4;

    case 0xB8: z80_cp_a(cpu, Z80_B); return 4;
    case 0xB9: z80_cp_a(cpu, Z80_C); return 4;
    case 0xBA: z80_cp_a(cpu, Z80_D); return 4;
    case 0xBB: z80_cp_a(cpu, Z80_E); return 4;
    case 0xBC: z80_cp_a(cpu, Z80_H); return 4;
    case 0xBD: z80_cp_a(cpu, Z80_L); return 4;
    case 0xBE: z80_cp_a(cpu, rd(cpu, Z80_HL)); return 7;
    case 0xBF: z80_cp_a(cpu, Z80_A); return 4;

    /* === ALU A,n === */
    case 0xC6: z80_add_a(cpu, fetch_byte(cpu)); return 7;
    case 0xCE: z80_adc_a(cpu, fetch_byte(cpu)); return 7;
    case 0xD6: z80_sub_a(cpu, fetch_byte(cpu)); return 7;
    case 0xDE: z80_sbc_a(cpu, fetch_byte(cpu)); return 7;
    case 0xE6: z80_and_a(cpu, fetch_byte(cpu)); return 7;
    case 0xEE: z80_xor_a(cpu, fetch_byte(cpu)); return 7;
    case 0xF6: z80_or_a(cpu, fetch_byte(cpu));  return 7;
    case 0xFE: z80_cp_a(cpu, fetch_byte(cpu));  return 7;

    /* === RET cc === */
    case 0xC0: if (!(Z80_F & Z80_FLAG_Z))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xC8: if ( (Z80_F & Z80_FLAG_Z))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xD0: if (!(Z80_F & Z80_FLAG_C))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xD8: if ( (Z80_F & Z80_FLAG_C))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xE0: if (!(Z80_F & Z80_FLAG_PV)) { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xE8: if ( (Z80_F & Z80_FLAG_PV)) { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xF0: if (!(Z80_F & Z80_FLAG_S))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;
    case 0xF8: if ( (Z80_F & Z80_FLAG_S))  { Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 11; } return 5;

    /* === RET === */
    case 0xC9: Z80_PC = pop16(cpu); Z80_WZ = Z80_PC; return 10;

    /* === JP cc,nn === */
    case 0xC2: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_Z))  Z80_PC = addr; return 10; }
    case 0xCA: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_Z))  Z80_PC = addr; return 10; }
    case 0xD2: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_C))  Z80_PC = addr; return 10; }
    case 0xDA: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_C))  Z80_PC = addr; return 10; }
    case 0xE2: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_PV)) Z80_PC = addr; return 10; }
    case 0xEA: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_PV)) Z80_PC = addr; return 10; }
    case 0xF2: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_S))  Z80_PC = addr; return 10; }
    case 0xFA: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_S))  Z80_PC = addr; return 10; }

    /* === JP nn === */
    case 0xC3: { u16 addr = fetch_word(cpu); Z80_PC = addr; Z80_WZ = addr; return 10; }

    /* === JP (HL) === */
    case 0xE9: Z80_PC = Z80_HL; return 4;

    /* === CALL cc,nn === */
    case 0xC4: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_Z))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xCC: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_Z))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xD4: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_C))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xDC: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_C))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xE4: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_PV)) { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xEC: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_PV)) { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xF4: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if (!(Z80_F & Z80_FLAG_S))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }
    case 0xFC: { u16 addr = fetch_word(cpu); Z80_WZ = addr; if ( (Z80_F & Z80_FLAG_S))  { push16(cpu, Z80_PC); Z80_PC = addr; return 17; } return 10; }

    /* === CALL nn === */
    case 0xCD: { u16 addr = fetch_word(cpu); Z80_WZ = addr; push16(cpu, Z80_PC); Z80_PC = addr; return 17; }

    /* === RST n === */
    case 0xC7: push16(cpu, Z80_PC); Z80_PC = 0x00; Z80_WZ = Z80_PC; return 11;
    case 0xCF: push16(cpu, Z80_PC); Z80_PC = 0x08; Z80_WZ = Z80_PC; return 11;
    case 0xD7: push16(cpu, Z80_PC); Z80_PC = 0x10; Z80_WZ = Z80_PC; return 11;
    case 0xDF: push16(cpu, Z80_PC); Z80_PC = 0x18; Z80_WZ = Z80_PC; return 11;
    case 0xE7: push16(cpu, Z80_PC); Z80_PC = 0x20; Z80_WZ = Z80_PC; return 11;
    case 0xEF: push16(cpu, Z80_PC); Z80_PC = 0x28; Z80_WZ = Z80_PC; return 11;
    case 0xF7: push16(cpu, Z80_PC); Z80_PC = 0x30; Z80_WZ = Z80_PC; return 11;
    case 0xFF: push16(cpu, Z80_PC); Z80_PC = 0x38; Z80_WZ = Z80_PC; return 11;

    /* === PUSH/POP rr === */
    case 0xC1: Z80_BC = pop16(cpu); return 10;
    case 0xD1: Z80_DE = pop16(cpu); return 10;
    case 0xE1: Z80_HL = pop16(cpu); return 10;
    case 0xF1: Z80_AF = pop16(cpu); return 10;
    case 0xC5: push16(cpu, Z80_BC); return 11;
    case 0xD5: push16(cpu, Z80_DE); return 11;
    case 0xE5: push16(cpu, Z80_HL); return 11;
    case 0xF5: push16(cpu, Z80_AF); return 11;

    /* === OUT (n),A / IN A,(n) === */
    case 0xD3: {
        u8 n = fetch_byte(cpu);
        u16 port = (u16)(n | (Z80_A << 8));
        cpu->port_write(cpu->callback_ctx, port, Z80_A);
        Z80_WZ = (u16)((Z80_A << 8) | ((n + 1) & 0xFF));
        return 11;
    }
    case 0xDB: {
        u8 n = fetch_byte(cpu);
        u16 port = (u16)(n | (Z80_A << 8));
        Z80_A = cpu->port_read(cpu->callback_ctx, port);
        Z80_WZ = port + 1;
        return 11;
    }

    /* === EXX === */
    case 0xD9: {
        u16 tmp;
        tmp = Z80_BC; Z80_BC = cpu->bc2.w; cpu->bc2.w = tmp;
        tmp = Z80_DE; Z80_DE = cpu->de2.w; cpu->de2.w = tmp;
        tmp = Z80_HL; Z80_HL = cpu->hl2.w; cpu->hl2.w = tmp;
        return 4;
    }

    /* === EX (SP),HL === */
    case 0xE3: {
        u8 lo = rd(cpu, Z80_SP);
        u8 hi = rd(cpu, Z80_SP + 1);
        wr(cpu, Z80_SP, Z80_L);
        wr(cpu, Z80_SP + 1, Z80_H);
        Z80_L = lo; Z80_H = hi;
        Z80_WZ = Z80_HL;
        return 19;
    }

    /* === EX DE,HL === */
    case 0xEB: { u16 tmp = Z80_DE; Z80_DE = Z80_HL; Z80_HL = tmp; return 4; }

    /* === LD SP,HL === */
    case 0xF9: Z80_SP = Z80_HL; return 6;

    /* === DI / EI === */
    case 0xF3: cpu->iff1 = 0; cpu->iff2 = 0; return 4;
    case 0xFB: cpu->iff1 = 1; cpu->iff2 = 1; cpu->ei_delay = true; return 4;

    /* === Prefix opcodes === */
    case 0xCB: return 4 + z80_exec_cb(cpu);
    case 0xED: return 4 + z80_exec_ed(cpu);
    case 0xDD: return 4 + z80_exec_dd(cpu);
    case 0xFD: return 4 + z80_exec_fd(cpu);

    default:
        /* Undocumented NOPs (various unused opcodes) */
        return 4;
    }

    return 4;
}
