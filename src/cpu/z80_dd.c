/*
 * SPEmulator — Z80 DD-prefixed opcodes (IX instructions)
 * Also used for FD prefix (IY) via z80_exec_fd which calls the same logic.
 */
#include "z80.h"

static inline u8 rd(z80_t *cpu, u16 addr) { return cpu->mem_read(cpu->callback_ctx, addr); }
static inline void wr(z80_t *cpu, u16 addr, u8 data) { cpu->mem_write(cpu->callback_ctx, addr, data); }
static inline u8 fetch_byte(z80_t *cpu) { u8 v = rd(cpu, Z80_PC); Z80_PC++; return v; }
static inline u16 fetch_word(z80_t *cpu) { u8 lo = fetch_byte(cpu); u8 hi = fetch_byte(cpu); return (u16)(lo | (hi << 8)); }
static inline void push16(z80_t *cpu, u16 val) { cpu->sp.w--; wr(cpu, Z80_SP, (u8)(val >> 8)); cpu->sp.w--; wr(cpu, Z80_SP, (u8)(val & 0xFF)); }
static inline u16 pop16(z80_t *cpu) { u8 lo = rd(cpu, Z80_SP); cpu->sp.w++; u8 hi = rd(cpu, Z80_SP); cpu->sp.w++; return (u16)(lo | (hi << 8)); }

/* Common IX/IY logic. `ir` points to IX or IY register. */
static int exec_dd_fd(z80_t *cpu, u16 *ir) {
    u8 opcode = rd(cpu, Z80_PC);
    Z80_PC++;
    cpu->r = (cpu->r & 0x80) | ((cpu->r + 1) & 0x7f);

    /* Pointers to high/low bytes of index register */
    u8 *irh, *irl;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    irl = (u8 *)ir;
    irh = ((u8 *)ir) + 1;
#else
    irh = (u8 *)ir;
    irl = ((u8 *)ir) + 1;
#endif

    switch (opcode) {
    /* LD IX,nn */
    case 0x21: *ir = fetch_word(cpu); return 14;
    /* LD (nn),IX */
    case 0x22: { u16 addr = fetch_word(cpu); wr(cpu, addr, *irl); wr(cpu, addr+1, *irh); Z80_WZ = addr+1; return 20; }
    /* LD IX,(nn) */
    case 0x2A: { u16 addr = fetch_word(cpu); *irl = rd(cpu, addr); *irh = rd(cpu, addr+1); Z80_WZ = addr+1; return 20; }
    /* INC IX */
    case 0x23: (*ir)++; return 10;
    /* DEC IX */
    case 0x2B: (*ir)--; return 10;

    /* ADD IX,rr */
    case 0x09: case 0x19: case 0x29: case 0x39: {
        u16 val;
        switch ((opcode >> 4) & 3) {
        case 0: val = Z80_BC; break;
        case 1: val = Z80_DE; break;
        case 2: val = *ir; break;
        default: val = Z80_SP; break;
        }
        u32 result = (u32)*ir + val;
        Z80_WZ = *ir + 1;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV))
              | ((result >> 16) & Z80_FLAG_C)
              | ((result >> 8) & (Z80_FLAG_5 | Z80_FLAG_3))
              | (((*ir ^ val ^ (u16)result) & 0x1000) ? Z80_FLAG_H : 0);
        *ir = (u16)result;
        return 15;
    }

    /* INC/DEC IXH (undocumented) */
    case 0x24: {
        (*irh)++;
        Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53_table[*irh]
              | ((*irh == 0x80) ? Z80_FLAG_PV : 0)
              | ((*irh & 0x0F) ? 0 : Z80_FLAG_H);
        return 8;
    }
    case 0x25: {
        Z80_F = (Z80_F & Z80_FLAG_C) | (((*irh & 0x0F) == 0) ? Z80_FLAG_H : 0)
              | ((*irh == 0x80) ? Z80_FLAG_PV : 0);
        (*irh)--;
        Z80_F |= Z80_FLAG_N | z80_sz53_table[*irh];
        return 8;
    }
    /* LD IXH,n (undocumented) */
    case 0x26: *irh = fetch_byte(cpu); return 11;

    /* INC/DEC IXL (undocumented) */
    case 0x2C: {
        (*irl)++;
        Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53_table[*irl]
              | ((*irl == 0x80) ? Z80_FLAG_PV : 0)
              | ((*irl & 0x0F) ? 0 : Z80_FLAG_H);
        return 8;
    }
    case 0x2D: {
        Z80_F = (Z80_F & Z80_FLAG_C) | (((*irl & 0x0F) == 0) ? Z80_FLAG_H : 0)
              | ((*irl == 0x80) ? Z80_FLAG_PV : 0);
        (*irl)--;
        Z80_F |= Z80_FLAG_N | z80_sz53_table[*irl];
        return 8;
    }
    /* LD IXL,n (undocumented) */
    case 0x2E: *irl = fetch_byte(cpu); return 11;

    /* INC/DEC (IX+d) */
    case 0x34: { i8 d = (i8)fetch_byte(cpu); u16 addr = *ir + d; Z80_WZ = addr; u8 v = rd(cpu, addr);
        v++; Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53_table[v] | ((v == 0x80) ? Z80_FLAG_PV : 0) | ((v & 0x0F) ? 0 : Z80_FLAG_H);
        wr(cpu, addr, v); return 23; }
    case 0x35: { i8 d = (i8)fetch_byte(cpu); u16 addr = *ir + d; Z80_WZ = addr; u8 v = rd(cpu, addr);
        Z80_F = (Z80_F & Z80_FLAG_C) | (((v & 0x0F) == 0) ? Z80_FLAG_H : 0) | ((v == 0x80) ? Z80_FLAG_PV : 0);
        v--; Z80_F |= Z80_FLAG_N | z80_sz53_table[v]; wr(cpu, addr, v); return 23; }

    /* LD (IX+d),n */
    case 0x36: { i8 d = (i8)fetch_byte(cpu); u8 n = fetch_byte(cpu); u16 addr = *ir + d; Z80_WZ = addr; wr(cpu, addr, n); return 19; }

    /* LD r,(IX+d) and LD (IX+d),r */
    case 0x46: case 0x4E: case 0x56: case 0x5E:
    case 0x66: case 0x6E: case 0x7E: {
        i8 d = (i8)fetch_byte(cpu);
        u16 addr = *ir + d;
        Z80_WZ = addr;
        u8 val = rd(cpu, addr);
        int dst = (opcode >> 3) & 7;
        switch (dst) {
        case 0: Z80_B = val; break; case 1: Z80_C = val; break;
        case 2: Z80_D = val; break; case 3: Z80_E = val; break;
        case 4: Z80_H = val; break; case 5: Z80_L = val; break;
        case 7: Z80_A = val; break;
        }
        return 19;
    }
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x77: {
        i8 d = (i8)fetch_byte(cpu);
        u16 addr = *ir + d;
        Z80_WZ = addr;
        int src = opcode & 7;
        u8 val;
        switch (src) {
        case 0: val = Z80_B; break; case 1: val = Z80_C; break;
        case 2: val = Z80_D; break; case 3: val = Z80_E; break;
        case 4: val = Z80_H; break; case 5: val = Z80_L; break;
        default: val = Z80_A; break;
        }
        wr(cpu, addr, val);
        return 19;
    }

    /* ALU A,(IX+d) */
    case 0x86: case 0x8E: case 0x96: case 0x9E:
    case 0xA6: case 0xAE: case 0xB6: case 0xBE: {
        i8 d = (i8)fetch_byte(cpu);
        u16 addr = *ir + d;
        Z80_WZ = addr;
        u8 val = rd(cpu, addr);
        u8 a = Z80_A;
        int op = (opcode >> 3) & 7;
        /* Inline ALU ops to avoid code duplication issues */
        switch (op) {
        case 0: { /* ADD */
            u16 r = (u16)a + val;
            Z80_F = ((r >> 8) & Z80_FLAG_C) | z80_sz53_table[(u8)r]
                  | (((a ^ val ^ (u8)r) & 0x10) ? Z80_FLAG_H : 0)
                  | ((((a ^ val) ^ 0x80) & (a ^ (u8)r) & 0x80) ? Z80_FLAG_PV : 0);
            Z80_A = (u8)r;
            break;
        }
        case 1: { /* ADC */
            u8 c = Z80_F & Z80_FLAG_C;
            u16 r = (u16)a + val + c;
            Z80_F = ((r >> 8) & Z80_FLAG_C) | z80_sz53_table[(u8)r]
                  | (((a ^ val ^ (u8)r) & 0x10) ? Z80_FLAG_H : 0)
                  | ((((a ^ val) ^ 0x80) & (a ^ (u8)r) & 0x80) ? Z80_FLAG_PV : 0);
            Z80_A = (u8)r;
            break;
        }
        case 2: { /* SUB */
            u16 r = (u16)a - val;
            Z80_F = ((r >> 8) & Z80_FLAG_C) | Z80_FLAG_N | z80_sz53_table[(u8)r]
                  | (((a ^ val ^ (u8)r) & 0x10) ? Z80_FLAG_H : 0)
                  | (((a ^ val) & (a ^ (u8)r) & 0x80) ? Z80_FLAG_PV : 0);
            Z80_A = (u8)r;
            break;
        }
        case 3: { /* SBC */
            u8 c = Z80_F & Z80_FLAG_C;
            u16 r = (u16)a - val - c;
            Z80_F = ((r >> 8) & Z80_FLAG_C) | Z80_FLAG_N | z80_sz53_table[(u8)r]
                  | (((a ^ val ^ (u8)r) & 0x10) ? Z80_FLAG_H : 0)
                  | (((a ^ val) & (a ^ (u8)r) & 0x80) ? Z80_FLAG_PV : 0);
            Z80_A = (u8)r;
            break;
        }
        case 4: Z80_A &= val; Z80_F = z80_sz53p_table[Z80_A] | Z80_FLAG_H; break; /* AND */
        case 5: Z80_A ^= val; Z80_F = z80_sz53p_table[Z80_A]; break; /* XOR */
        case 6: Z80_A |= val; Z80_F = z80_sz53p_table[Z80_A]; break; /* OR */
        case 7: { /* CP */
            u16 r = (u16)a - val;
            Z80_F = ((r >> 8) & Z80_FLAG_C) | Z80_FLAG_N
                  | ((u8)r & Z80_FLAG_S) | (((u8)r == 0) ? Z80_FLAG_Z : 0)
                  | (val & (Z80_FLAG_5 | Z80_FLAG_3))
                  | (((a ^ val ^ (u8)r) & 0x10) ? Z80_FLAG_H : 0)
                  | (((a ^ val) & (a ^ (u8)r) & 0x80) ? Z80_FLAG_PV : 0);
            break;
        }
        }
        return 19;
    }

    /* PUSH/POP IX */
    case 0xE1: *ir = pop16(cpu); return 14;
    case 0xE5: push16(cpu, *ir); return 15;

    /* JP (IX) */
    case 0xE9: Z80_PC = *ir; return 8;

    /* LD SP,IX */
    case 0xF9: Z80_SP = *ir; return 10;

    /* EX (SP),IX */
    case 0xE3: {
        u8 lo = rd(cpu, Z80_SP);
        u8 hi = rd(cpu, Z80_SP + 1);
        wr(cpu, Z80_SP, (u8)(*ir & 0xFF));
        wr(cpu, Z80_SP + 1, (u8)(*ir >> 8));
        *ir = (u16)(lo | (hi << 8));
        Z80_WZ = *ir;
        return 23;
    }

    /* DD CB prefix */
    case 0xCB: {
        i8 d = (i8)fetch_byte(cpu);
        Z80_WZ = *ir + d;
        if (ir == &Z80_IX)
            return 4 + z80_exec_ddcb(cpu, d);
        else
            return 4 + z80_exec_fdcb(cpu, d);
    }

    /* Undocumented: LD IXH/IXL,r and LD r,IXH/IXL */
    /* These are handled in the 0x40-0x7F range for non-(IX+d) cases */
    case 0x44: Z80_B = *irh; return 8;
    case 0x45: Z80_B = *irl; return 8;
    case 0x4C: Z80_C = *irh; return 8;
    case 0x4D: Z80_C = *irl; return 8;
    case 0x54: Z80_D = *irh; return 8;
    case 0x55: Z80_D = *irl; return 8;
    case 0x5C: Z80_E = *irh; return 8;
    case 0x5D: Z80_E = *irl; return 8;
    case 0x60: *irh = Z80_B; return 8;
    case 0x61: *irh = Z80_C; return 8;
    case 0x62: *irh = Z80_D; return 8;
    case 0x63: *irh = Z80_E; return 8;
    case 0x64: /* LD IXH,IXH */ return 8;
    case 0x65: *irh = *irl; return 8;
    case 0x67: *irh = Z80_A; return 8;
    case 0x68: *irl = Z80_B; return 8;
    case 0x69: *irl = Z80_C; return 8;
    case 0x6A: *irl = Z80_D; return 8;
    case 0x6B: *irl = Z80_E; return 8;
    case 0x6C: *irl = *irh; return 8;
    case 0x6D: /* LD IXL,IXL */ return 8;
    case 0x6F: *irl = Z80_A; return 8;
    case 0x7C: Z80_A = *irh; return 8;
    case 0x7D: Z80_A = *irl; return 8;

    /* Undocumented ALU with IXH/IXL */
    case 0x84: { u8 a = Z80_A; u16 r = (u16)a + *irh; Z80_F = ((r>>8)&1)|z80_sz53_table[(u8)r]|(((a^*irh^(u8)r)&0x10)?Z80_FLAG_H:0)|((((a^*irh)^0x80)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x85: { u8 a = Z80_A; u16 r = (u16)a + *irl; Z80_F = ((r>>8)&1)|z80_sz53_table[(u8)r]|(((a^*irl^(u8)r)&0x10)?Z80_FLAG_H:0)|((((a^*irl)^0x80)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x8C: { u8 a=Z80_A,c=Z80_F&1; u16 r=(u16)a+*irh+c; Z80_F=((r>>8)&1)|z80_sz53_table[(u8)r]|(((a^*irh^(u8)r)&0x10)?Z80_FLAG_H:0)|((((a^*irh)^0x80)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x8D: { u8 a=Z80_A,c=Z80_F&1; u16 r=(u16)a+*irl+c; Z80_F=((r>>8)&1)|z80_sz53_table[(u8)r]|(((a^*irl^(u8)r)&0x10)?Z80_FLAG_H:0)|((((a^*irl)^0x80)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x94: { u8 a=Z80_A; u16 r=(u16)a-*irh; Z80_F=((r>>8)&1)|2|z80_sz53_table[(u8)r]|(((a^*irh^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irh)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x95: { u8 a=Z80_A; u16 r=(u16)a-*irl; Z80_F=((r>>8)&1)|2|z80_sz53_table[(u8)r]|(((a^*irl^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irl)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x9C: { u8 a=Z80_A,c=Z80_F&1; u16 r=(u16)a-*irh-c; Z80_F=((r>>8)&1)|2|z80_sz53_table[(u8)r]|(((a^*irh^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irh)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0x9D: { u8 a=Z80_A,c=Z80_F&1; u16 r=(u16)a-*irl-c; Z80_F=((r>>8)&1)|2|z80_sz53_table[(u8)r]|(((a^*irl^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irl)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); Z80_A=(u8)r; return 8; }
    case 0xA4: Z80_A &= *irh; Z80_F = z80_sz53p_table[Z80_A] | Z80_FLAG_H; return 8;
    case 0xA5: Z80_A &= *irl; Z80_F = z80_sz53p_table[Z80_A] | Z80_FLAG_H; return 8;
    case 0xAC: Z80_A ^= *irh; Z80_F = z80_sz53p_table[Z80_A]; return 8;
    case 0xAD: Z80_A ^= *irl; Z80_F = z80_sz53p_table[Z80_A]; return 8;
    case 0xB4: Z80_A |= *irh; Z80_F = z80_sz53p_table[Z80_A]; return 8;
    case 0xB5: Z80_A |= *irl; Z80_F = z80_sz53p_table[Z80_A]; return 8;
    case 0xBC: { u8 a=Z80_A; u16 r=(u16)a-*irh; Z80_F=((r>>8)&1)|2|((u8)r&0x80)|(((u8)r==0)?0x40:0)|(*irh&0x28)|(((a^*irh^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irh)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); return 8; }
    case 0xBD: { u8 a=Z80_A; u16 r=(u16)a-*irl; Z80_F=((r>>8)&1)|2|((u8)r&0x80)|(((u8)r==0)?0x40:0)|(*irl&0x28)|(((a^*irl^(u8)r)&0x10)?Z80_FLAG_H:0)|(((a^*irl)&(a^(u8)r)&0x80)?Z80_FLAG_PV:0); return 8; }

    default:
        /* For unhandled DD opcodes, execute as unprefixed
           (the DD prefix acts as NOP effectively) */
        /* We already consumed 4 T-states for the prefix read.
           Re-execute the opcode as a normal instruction. */
        Z80_PC--; /* back up to re-read opcode */
        return 4; /* just the prefix cost */
    }
}

int z80_exec_dd(z80_t *cpu) {
    return exec_dd_fd(cpu, &Z80_IX);
}

int z80_exec_fd(z80_t *cpu) {
    return exec_dd_fd(cpu, &Z80_IY);
}
