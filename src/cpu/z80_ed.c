/*
 * SPEmulator — Z80 ED-prefixed opcodes
 * Block operations, I/O, 16-bit arithmetic, etc.
 */
#include "z80.h"

static inline u8 rd(z80_t *cpu, u16 addr) { return cpu->mem_read(cpu->callback_ctx, addr); }
static inline void wr(z80_t *cpu, u16 addr, u8 data) { cpu->mem_write(cpu->callback_ctx, addr, data); }

int z80_exec_ed(z80_t *cpu) {
    u8 opcode = cpu->mem_read(cpu->callback_ctx, Z80_PC);
    Z80_PC++;
    cpu->r = (cpu->r & 0x80) | ((cpu->r + 1) & 0x7f);

    switch (opcode) {
    /* === IN r,(C) === */
    case 0x40: Z80_B = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_B]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x48: Z80_C = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_C]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x50: Z80_D = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_D]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x58: Z80_E = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_E]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x60: Z80_H = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_H]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x68: Z80_L = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_L]; Z80_WZ = Z80_BC + 1; return 12;
    case 0x70: { u8 v = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[v]; Z80_WZ = Z80_BC + 1; return 12; } /* IN (C) - undocumented */
    case 0x78: Z80_A = cpu->port_read(cpu->callback_ctx, Z80_BC); Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_A]; Z80_WZ = Z80_BC + 1; return 12;

    /* === OUT (C),r === */
    case 0x41: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_B); Z80_WZ = Z80_BC + 1; return 12;
    case 0x49: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_C); Z80_WZ = Z80_BC + 1; return 12;
    case 0x51: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_D); Z80_WZ = Z80_BC + 1; return 12;
    case 0x59: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_E); Z80_WZ = Z80_BC + 1; return 12;
    case 0x61: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_H); Z80_WZ = Z80_BC + 1; return 12;
    case 0x69: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_L); Z80_WZ = Z80_BC + 1; return 12;
    case 0x71: cpu->port_write(cpu->callback_ctx, Z80_BC, 0); Z80_WZ = Z80_BC + 1; return 12; /* OUT (C),0 - undocumented */
    case 0x79: cpu->port_write(cpu->callback_ctx, Z80_BC, Z80_A); Z80_WZ = Z80_BC + 1; return 12;

    /* === SBC HL,rr === */
    case 0x42: case 0x52: case 0x62: case 0x72: {
        u16 *rr;
        switch ((opcode >> 4) & 3) {
        case 0: rr = &Z80_BC; break;
        case 1: rr = &Z80_DE; break;
        case 2: rr = &Z80_HL; break;
        default: rr = &Z80_SP; break;
        }
        u16 val = *rr;
        u8 c = Z80_F & Z80_FLAG_C;
        u32 result = (u32)Z80_HL - val - c;
        Z80_WZ = Z80_HL + 1;
        Z80_F = ((result >> 16) & Z80_FLAG_C)
              | Z80_FLAG_N
              | ((result >> 8) & (Z80_FLAG_S | Z80_FLAG_5 | Z80_FLAG_3))
              | (((u16)result == 0) ? Z80_FLAG_Z : 0)
              | (((Z80_HL ^ val) & (Z80_HL ^ (u16)result) & 0x8000) ? Z80_FLAG_PV : 0)
              | (((Z80_HL ^ val ^ (u16)result) & 0x1000) ? Z80_FLAG_H : 0);
        Z80_HL = (u16)result;
        return 15;
    }

    /* === ADC HL,rr === */
    case 0x4A: case 0x5A: case 0x6A: case 0x7A: {
        u16 *rr;
        switch ((opcode >> 4) & 3) {
        case 0: rr = &Z80_BC; break;
        case 1: rr = &Z80_DE; break;
        case 2: rr = &Z80_HL; break;
        default: rr = &Z80_SP; break;
        }
        u16 val = *rr;
        u8 c = Z80_F & Z80_FLAG_C;
        u32 result = (u32)Z80_HL + val + c;
        Z80_WZ = Z80_HL + 1;
        Z80_F = ((result >> 16) & Z80_FLAG_C)
              | ((result >> 8) & (Z80_FLAG_S | Z80_FLAG_5 | Z80_FLAG_3))
              | (((u16)result == 0) ? Z80_FLAG_Z : 0)
              | ((((Z80_HL ^ val) ^ 0x8000) & ((u16)result ^ Z80_HL) & 0x8000) ? Z80_FLAG_PV : 0)
              | (((Z80_HL ^ val ^ (u16)result) & 0x1000) ? Z80_FLAG_H : 0);
        Z80_HL = (u16)result;
        return 15;
    }

    /* === LD (nn),rr / LD rr,(nn) === */
    case 0x43: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; wr(cpu, addr, Z80_C); wr(cpu, addr+1, Z80_B); Z80_WZ = addr+1; return 20; }
    case 0x53: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; wr(cpu, addr, Z80_E); wr(cpu, addr+1, Z80_D); Z80_WZ = addr+1; return 20; }
    case 0x63: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; wr(cpu, addr, Z80_L); wr(cpu, addr+1, Z80_H); Z80_WZ = addr+1; return 20; }
    case 0x73: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; wr(cpu, addr, (u8)(Z80_SP & 0xFF)); wr(cpu, addr+1, (u8)(Z80_SP >> 8)); Z80_WZ = addr+1; return 20; }
    case 0x4B: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; Z80_C = rd(cpu, addr); Z80_B = rd(cpu, addr+1); Z80_WZ = addr+1; return 20; }
    case 0x5B: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; Z80_E = rd(cpu, addr); Z80_D = rd(cpu, addr+1); Z80_WZ = addr+1; return 20; }
    case 0x6B: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; Z80_L = rd(cpu, addr); Z80_H = rd(cpu, addr+1); Z80_WZ = addr+1; return 20; }
    case 0x7B: { u16 addr = rd(cpu, Z80_PC) | (rd(cpu, Z80_PC+1) << 8); Z80_PC += 2; u8 lo = rd(cpu, addr); u8 hi = rd(cpu, addr+1); Z80_SP = lo | (hi << 8); Z80_WZ = addr+1; return 20; }

    /* === NEG === */
    case 0x44: case 0x4C: case 0x54: case 0x5C:
    case 0x64: case 0x6C: case 0x74: case 0x7C: {
        u8 a = Z80_A;
        Z80_A = 0;
        u16 result = (u16)0 - a;
        Z80_A = (u8)result;
        Z80_F = ((result >> 8) & Z80_FLAG_C)
              | Z80_FLAG_N
              | z80_sz53_table[Z80_A]
              | ((a & 0x0F) ? Z80_FLAG_H : 0)
              | ((a == 0x80) ? Z80_FLAG_PV : 0);
        if (a) Z80_F |= Z80_FLAG_C;
        return 8;
    }

    /* === RETN === */
    case 0x45: case 0x55: case 0x65: case 0x75:
        cpu->iff1 = cpu->iff2;
        Z80_PC = rd(cpu, Z80_SP) | (rd(cpu, Z80_SP + 1) << 8);
        Z80_SP += 2;
        Z80_WZ = Z80_PC;
        return 14;

    /* === RETI === */
    case 0x4D: case 0x5D: case 0x6D: case 0x7D:
        cpu->iff1 = cpu->iff2;
        Z80_PC = rd(cpu, Z80_SP) | (rd(cpu, Z80_SP + 1) << 8);
        Z80_SP += 2;
        Z80_WZ = Z80_PC;
        return 14;

    /* === IM n === */
    case 0x46: case 0x66: cpu->im = 0; return 8;
    case 0x56: case 0x76: cpu->im = 1; return 8;
    case 0x5E: case 0x7E: cpu->im = 2; return 8;
    case 0x4E: case 0x6E: cpu->im = 0; return 8; /* undocumented IM 0 */

    /* === LD I,A / LD R,A / LD A,I / LD A,R === */
    case 0x47: cpu->i = Z80_A; return 9;
    case 0x4F: cpu->r = Z80_A; cpu->r7 = Z80_A & 0x80; return 9;
    case 0x57: /* LD A,I */
        Z80_A = cpu->i;
        Z80_F = (Z80_F & Z80_FLAG_C)
              | z80_sz53_table[Z80_A]
              | (cpu->iff2 ? Z80_FLAG_PV : 0);
        return 9;
    case 0x5F: /* LD A,R */
        Z80_A = (cpu->r & 0x7F) | cpu->r7;
        Z80_F = (Z80_F & Z80_FLAG_C)
              | z80_sz53_table[Z80_A]
              | (cpu->iff2 ? Z80_FLAG_PV : 0);
        return 9;

    /* === RRD / RLD === */
    case 0x67: { /* RRD */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_HL, (v >> 4) | (Z80_A << 4));
        Z80_A = (Z80_A & 0xF0) | (v & 0x0F);
        Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_A];
        Z80_WZ = Z80_HL + 1;
        return 18;
    }
    case 0x6F: { /* RLD */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_HL, (v << 4) | (Z80_A & 0x0F));
        Z80_A = (Z80_A & 0xF0) | (v >> 4);
        Z80_F = (Z80_F & Z80_FLAG_C) | z80_sz53p_table[Z80_A];
        Z80_WZ = Z80_HL + 1;
        return 18;
    }

    /* === Block transfer: LDI, LDD, LDIR, LDDR === */
    case 0xA0: { /* LDI */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_DE, v);
        Z80_HL++; Z80_DE++; Z80_BC--;
        u8 n = v + Z80_A;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_C))
              | (Z80_BC ? Z80_FLAG_PV : 0)
              | (n & Z80_FLAG_3)
              | ((n & 0x02) ? Z80_FLAG_5 : 0);
        return 16;
    }
    case 0xA8: { /* LDD */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_DE, v);
        Z80_HL--; Z80_DE--; Z80_BC--;
        u8 n = v + Z80_A;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_C))
              | (Z80_BC ? Z80_FLAG_PV : 0)
              | (n & Z80_FLAG_3)
              | ((n & 0x02) ? Z80_FLAG_5 : 0);
        return 16;
    }
    case 0xB0: { /* LDIR */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_DE, v);
        Z80_HL++; Z80_DE++; Z80_BC--;
        u8 n = v + Z80_A;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_C))
              | (n & Z80_FLAG_3)
              | ((n & 0x02) ? Z80_FLAG_5 : 0);
        if (Z80_BC) {
            Z80_PC -= 2; /* repeat */
            Z80_WZ = Z80_PC + 1;
            Z80_F |= Z80_FLAG_PV;
            return 21;
        }
        return 16;
    }
    case 0xB8: { /* LDDR */
        u8 v = rd(cpu, Z80_HL);
        wr(cpu, Z80_DE, v);
        Z80_HL--; Z80_DE--; Z80_BC--;
        u8 n = v + Z80_A;
        Z80_F = (Z80_F & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_C))
              | (n & Z80_FLAG_3)
              | ((n & 0x02) ? Z80_FLAG_5 : 0);
        if (Z80_BC) {
            Z80_PC -= 2;
            Z80_WZ = Z80_PC + 1;
            Z80_F |= Z80_FLAG_PV;
            return 21;
        }
        return 16;
    }

    /* === Block compare: CPI, CPD, CPIR, CPDR === */
    case 0xA1: { /* CPI */
        u8 v = rd(cpu, Z80_HL);
        u8 result = Z80_A - v;
        Z80_HL++; Z80_BC--;
        u8 n = Z80_A ^ v ^ result;
        Z80_F = (Z80_F & Z80_FLAG_C) | Z80_FLAG_N
              | (result & Z80_FLAG_S)
              | ((result == 0) ? Z80_FLAG_Z : 0)
              | (n & Z80_FLAG_H)
              | (Z80_BC ? Z80_FLAG_PV : 0);
        u8 tmp = result - ((Z80_F & Z80_FLAG_H) ? 1 : 0);
        Z80_F |= (tmp & Z80_FLAG_3) | ((tmp & 0x02) ? Z80_FLAG_5 : 0);
        Z80_WZ++;
        return 16;
    }
    case 0xA9: { /* CPD */
        u8 v = rd(cpu, Z80_HL);
        u8 result = Z80_A - v;
        Z80_HL--; Z80_BC--;
        u8 n = Z80_A ^ v ^ result;
        Z80_F = (Z80_F & Z80_FLAG_C) | Z80_FLAG_N
              | (result & Z80_FLAG_S)
              | ((result == 0) ? Z80_FLAG_Z : 0)
              | (n & Z80_FLAG_H)
              | (Z80_BC ? Z80_FLAG_PV : 0);
        u8 tmp = result - ((Z80_F & Z80_FLAG_H) ? 1 : 0);
        Z80_F |= (tmp & Z80_FLAG_3) | ((tmp & 0x02) ? Z80_FLAG_5 : 0);
        Z80_WZ--;
        return 16;
    }
    case 0xB1: { /* CPIR */
        u8 v = rd(cpu, Z80_HL);
        u8 result = Z80_A - v;
        Z80_HL++; Z80_BC--;
        u8 n = Z80_A ^ v ^ result;
        Z80_F = (Z80_F & Z80_FLAG_C) | Z80_FLAG_N
              | (result & Z80_FLAG_S)
              | ((result == 0) ? Z80_FLAG_Z : 0)
              | (n & Z80_FLAG_H)
              | (Z80_BC ? Z80_FLAG_PV : 0);
        u8 tmp = result - ((Z80_F & Z80_FLAG_H) ? 1 : 0);
        Z80_F |= (tmp & Z80_FLAG_3) | ((tmp & 0x02) ? Z80_FLAG_5 : 0);
        if (Z80_BC && result) {
            Z80_PC -= 2;
            Z80_WZ = Z80_PC + 1;
            return 21;
        }
        Z80_WZ++;
        return 16;
    }
    case 0xB9: { /* CPDR */
        u8 v = rd(cpu, Z80_HL);
        u8 result = Z80_A - v;
        Z80_HL--; Z80_BC--;
        u8 n = Z80_A ^ v ^ result;
        Z80_F = (Z80_F & Z80_FLAG_C) | Z80_FLAG_N
              | (result & Z80_FLAG_S)
              | ((result == 0) ? Z80_FLAG_Z : 0)
              | (n & Z80_FLAG_H)
              | (Z80_BC ? Z80_FLAG_PV : 0);
        u8 tmp = result - ((Z80_F & Z80_FLAG_H) ? 1 : 0);
        Z80_F |= (tmp & Z80_FLAG_3) | ((tmp & 0x02) ? Z80_FLAG_5 : 0);
        if (Z80_BC && result) {
            Z80_PC -= 2;
            Z80_WZ = Z80_PC + 1;
            return 21;
        }
        Z80_WZ--;
        return 16;
    }

    /* === Block I/O: INI, IND, INIR, INDR, OUTI, OUTD, OTIR, OTDR === */
    case 0xA2: { /* INI */
        Z80_WZ = Z80_BC + 1;
        u8 v = cpu->port_read(cpu->callback_ctx, Z80_BC);
        wr(cpu, Z80_HL, v);
        Z80_B--;
        Z80_HL++;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N; /* simplified */
        return 16;
    }
    case 0xAA: { /* IND */
        Z80_WZ = Z80_BC - 1;
        u8 v = cpu->port_read(cpu->callback_ctx, Z80_BC);
        wr(cpu, Z80_HL, v);
        Z80_B--;
        Z80_HL--;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        return 16;
    }
    case 0xB2: { /* INIR */
        Z80_WZ = Z80_BC + 1;
        u8 v = cpu->port_read(cpu->callback_ctx, Z80_BC);
        wr(cpu, Z80_HL, v);
        Z80_B--;
        Z80_HL++;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        if (Z80_B) { Z80_PC -= 2; return 21; }
        return 16;
    }
    case 0xBA: { /* INDR */
        Z80_WZ = Z80_BC - 1;
        u8 v = cpu->port_read(cpu->callback_ctx, Z80_BC);
        wr(cpu, Z80_HL, v);
        Z80_B--;
        Z80_HL--;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        if (Z80_B) { Z80_PC -= 2; return 21; }
        return 16;
    }
    case 0xA3: { /* OUTI */
        u8 v = rd(cpu, Z80_HL);
        Z80_B--;
        cpu->port_write(cpu->callback_ctx, Z80_BC, v);
        Z80_HL++;
        Z80_WZ = Z80_BC + 1;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        return 16;
    }
    case 0xAB: { /* OUTD */
        u8 v = rd(cpu, Z80_HL);
        Z80_B--;
        cpu->port_write(cpu->callback_ctx, Z80_BC, v);
        Z80_HL--;
        Z80_WZ = Z80_BC - 1;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        return 16;
    }
    case 0xB3: { /* OTIR */
        u8 v = rd(cpu, Z80_HL);
        Z80_B--;
        cpu->port_write(cpu->callback_ctx, Z80_BC, v);
        Z80_HL++;
        Z80_WZ = Z80_BC + 1;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        if (Z80_B) { Z80_PC -= 2; return 21; }
        return 16;
    }
    case 0xBB: { /* OTDR */
        u8 v = rd(cpu, Z80_HL);
        Z80_B--;
        cpu->port_write(cpu->callback_ctx, Z80_BC, v);
        Z80_HL--;
        Z80_WZ = Z80_BC - 1;
        Z80_F = z80_sz53_table[Z80_B] | Z80_FLAG_N;
        if (Z80_B) { Z80_PC -= 2; return 21; }
        return 16;
    }

    default:
        /* Undocumented ED NOPs */
        return 8;
    }
}
