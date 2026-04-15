/*
 * SPEmulator — Z80 CB-prefixed opcodes
 * Bit operations: RLC, RRC, RL, RR, SLA, SRA, SLL*, SRL, BIT, RES, SET
 */
#include "z80.h"

static inline u8 rd(z80_t *cpu, u16 addr) { return cpu->mem_read(cpu->callback_ctx, addr); }
static inline void wr(z80_t *cpu, u16 addr, u8 data) { cpu->mem_write(cpu->callback_ctx, addr, data); }

/* Rotate/shift operations */
static inline u8 op_rlc(z80_t *cpu, u8 val) {
    u8 c = val >> 7;
    val = (val << 1) | c;
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_rrc(z80_t *cpu, u8 val) {
    u8 c = val & 1;
    val = (val >> 1) | (c << 7);
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_rl(z80_t *cpu, u8 val) {
    u8 c = val >> 7;
    val = (val << 1) | (Z80_F & Z80_FLAG_C);
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_rr(z80_t *cpu, u8 val) {
    u8 c = val & 1;
    val = (val >> 1) | ((Z80_F & Z80_FLAG_C) << 7);
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_sla(z80_t *cpu, u8 val) {
    u8 c = val >> 7;
    val <<= 1;
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_sra(z80_t *cpu, u8 val) {
    u8 c = val & 1;
    val = (val >> 1) | (val & 0x80);
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_sll(z80_t *cpu, u8 val) {
    /* Undocumented: like SLA but sets bit 0 */
    u8 c = val >> 7;
    val = (val << 1) | 1;
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}
static inline u8 op_srl(z80_t *cpu, u8 val) {
    u8 c = val & 1;
    val >>= 1;
    Z80_F = z80_sz53p_table[val] | c;
    return val;
}

static inline void op_bit(z80_t *cpu, int bit, u8 val) {
    u8 result = val & (1 << bit);
    Z80_F = (Z80_F & Z80_FLAG_C)
          | Z80_FLAG_H
          | (result ? 0 : (Z80_FLAG_Z | Z80_FLAG_PV))
          | (result & Z80_FLAG_S)
          | (val & (Z80_FLAG_5 | Z80_FLAG_3));
}

/* Helper to get/set register by index */
static inline u8 get_r(z80_t *cpu, int r) {
    switch (r) {
    case 0: return Z80_B; case 1: return Z80_C;
    case 2: return Z80_D; case 3: return Z80_E;
    case 4: return Z80_H; case 5: return Z80_L;
    case 6: return rd(cpu, Z80_HL);
    case 7: return Z80_A;
    }
    return 0;
}
static inline void set_r(z80_t *cpu, int r, u8 val) {
    switch (r) {
    case 0: Z80_B = val; break; case 1: Z80_C = val; break;
    case 2: Z80_D = val; break; case 3: Z80_E = val; break;
    case 4: Z80_H = val; break; case 5: Z80_L = val; break;
    case 6: wr(cpu, Z80_HL, val); break;
    case 7: Z80_A = val; break;
    }
}

int z80_exec_cb(z80_t *cpu) {
    u8 opcode = cpu->mem_read(cpu->callback_ctx, Z80_PC);
    Z80_PC++;
    cpu->r = (cpu->r & 0x80) | ((cpu->r + 1) & 0x7f);

    int r = opcode & 0x07;
    int is_hl = (r == 6);
    int base_t = is_hl ? 15 : 8;

    u8 val = get_r(cpu, r);

    switch (opcode >> 3) {
    case 0: val = op_rlc(cpu, val); break;  /* RLC */
    case 1: val = op_rrc(cpu, val); break;  /* RRC */
    case 2: val = op_rl(cpu, val);  break;  /* RL */
    case 3: val = op_rr(cpu, val);  break;  /* RR */
    case 4: val = op_sla(cpu, val); break;  /* SLA */
    case 5: val = op_sra(cpu, val); break;  /* SRA */
    case 6: val = op_sll(cpu, val); break;  /* SLL (undocumented) */
    case 7: val = op_srl(cpu, val); break;  /* SRL */
    /* BIT b,r: opcodes 0x40-0x7F */
    case  8: case  9: case 10: case 11:
    case 12: case 13: case 14: case 15:
        op_bit(cpu, (opcode >> 3) & 7, val);
        /* BIT b,(HL) uses WZ bits 5,3 for undocumented flags */
        if (is_hl) {
            Z80_F = (Z80_F & ~(Z80_FLAG_5 | Z80_FLAG_3))
                  | (cpu->wz.hi & (Z80_FLAG_5 | Z80_FLAG_3));
        }
        return is_hl ? 12 : 8;
    /* RES b,r: opcodes 0x80-0xBF */
    case 16: case 17: case 18: case 19:
    case 20: case 21: case 22: case 23:
        val &= ~(1 << ((opcode >> 3) & 7));
        break;
    /* SET b,r: opcodes 0xC0-0xFF */
    case 24: case 25: case 26: case 27:
    case 28: case 29: case 30: case 31:
        val |= (1 << ((opcode >> 3) & 7));
        break;
    }

    set_r(cpu, r, val);
    return base_t;
}
