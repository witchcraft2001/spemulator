/*
 * SPEmulator — Z80 Disassembler
 * Handles all prefixes: CB, ED, DD, FD, DDCB, FDCB.
 * Includes undocumented instructions.
 */
#include "cpu/z80_disasm.h"
#include <stdio.h>
#include <string.h>

static const char *r8_names[8] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
static const char *r16_names[4] = { "BC", "DE", "HL", "SP" };
static const char *r16af_names[4] = { "BC", "DE", "HL", "AF" };
static const char *cc_names[8] = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
static const char *alu_names[8] = { "ADD A,", "ADC A,", "SUB ", "SBC A,", "AND ", "XOR ", "OR ", "CP " };
static const char *rot_names[8] = { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL" };

static int disasm_cb(const u8 *mem, u16 addr, char *buf) {
    u8 op = mem[addr];
    int r = op & 7;
    int bits = (op >> 3) & 7;

    if (op < 0x40) {
        snprintf(buf, 64, "%s %s", rot_names[bits], r8_names[r]);
    } else if (op < 0x80) {
        snprintf(buf, 64, "BIT %d,%s", bits, r8_names[r]);
    } else if (op < 0xC0) {
        snprintf(buf, 64, "RES %d,%s", bits, r8_names[r]);
    } else {
        snprintf(buf, 64, "SET %d,%s", bits, r8_names[r]);
    }
    return 1;
}

static int disasm_ed(const u8 *mem, u16 addr, char *buf) {
    u8 op = mem[addr];

    switch (op) {
    case 0x40: case 0x48: case 0x50: case 0x58:
    case 0x60: case 0x68: case 0x78:
        snprintf(buf, 64, "IN %s,(C)", r8_names[(op >> 3) & 7]);
        return 1;
    case 0x70: snprintf(buf, 64, "IN (C)"); return 1;
    case 0x41: case 0x49: case 0x51: case 0x59:
    case 0x61: case 0x69: case 0x79:
        snprintf(buf, 64, "OUT (C),%s", r8_names[(op >> 3) & 7]);
        return 1;
    case 0x71: snprintf(buf, 64, "OUT (C),0"); return 1;
    case 0x42: case 0x52: case 0x62: case 0x72:
        snprintf(buf, 64, "SBC HL,%s", r16_names[(op >> 4) & 3]);
        return 1;
    case 0x4A: case 0x5A: case 0x6A: case 0x7A:
        snprintf(buf, 64, "ADC HL,%s", r16_names[(op >> 4) & 3]);
        return 1;
    case 0x43: case 0x53: case 0x63: case 0x73: {
        u16 nn = mem[addr+1] | (mem[addr+2] << 8);
        snprintf(buf, 64, "LD (#%04X),%s", nn, r16_names[(op >> 4) & 3]);
        return 3;
    }
    case 0x4B: case 0x5B: case 0x6B: case 0x7B: {
        u16 nn = mem[addr+1] | (mem[addr+2] << 8);
        snprintf(buf, 64, "LD %s,(#%04X)", r16_names[(op >> 4) & 3], nn);
        return 3;
    }
    case 0x44: snprintf(buf, 64, "NEG"); return 1;
    case 0x45: snprintf(buf, 64, "RETN"); return 1;
    case 0x4D: snprintf(buf, 64, "RETI"); return 1;
    case 0x46: snprintf(buf, 64, "IM 0"); return 1;
    case 0x56: snprintf(buf, 64, "IM 1"); return 1;
    case 0x5E: snprintf(buf, 64, "IM 2"); return 1;
    case 0x47: snprintf(buf, 64, "LD I,A"); return 1;
    case 0x4F: snprintf(buf, 64, "LD R,A"); return 1;
    case 0x57: snprintf(buf, 64, "LD A,I"); return 1;
    case 0x5F: snprintf(buf, 64, "LD A,R"); return 1;
    case 0x67: snprintf(buf, 64, "RRD"); return 1;
    case 0x6F: snprintf(buf, 64, "RLD"); return 1;
    case 0xA0: snprintf(buf, 64, "LDI"); return 1;
    case 0xA8: snprintf(buf, 64, "LDD"); return 1;
    case 0xB0: snprintf(buf, 64, "LDIR"); return 1;
    case 0xB8: snprintf(buf, 64, "LDDR"); return 1;
    case 0xA1: snprintf(buf, 64, "CPI"); return 1;
    case 0xA9: snprintf(buf, 64, "CPD"); return 1;
    case 0xB1: snprintf(buf, 64, "CPIR"); return 1;
    case 0xB9: snprintf(buf, 64, "CPDR"); return 1;
    case 0xA2: snprintf(buf, 64, "INI"); return 1;
    case 0xAA: snprintf(buf, 64, "IND"); return 1;
    case 0xB2: snprintf(buf, 64, "INIR"); return 1;
    case 0xBA: snprintf(buf, 64, "INDR"); return 1;
    case 0xA3: snprintf(buf, 64, "OUTI"); return 1;
    case 0xAB: snprintf(buf, 64, "OUTD"); return 1;
    case 0xB3: snprintf(buf, 64, "OTIR"); return 1;
    case 0xBB: snprintf(buf, 64, "OTDR"); return 1;
    default:
        snprintf(buf, 64, "NOP ; ED %02X", op);
        return 1;
    }
}

static int disasm_ddfd(const u8 *mem, u16 addr, char *buf, const char *ir) {
    u8 op = mem[addr];

    if (op == 0xCB) {
        /* DDCB/FDCB: displacement then opcode */
        i8 d = (i8)mem[addr + 1];
        u8 cbop = mem[addr + 2];
        int bits = (cbop >> 3) & 7;
        char idxstr[32];
        snprintf(idxstr, sizeof(idxstr), "(%s%+d)", ir, d);

        if (cbop < 0x40) {
            snprintf(buf, 64, "%s %s", rot_names[bits], idxstr);
        } else if (cbop < 0x80) {
            snprintf(buf, 64, "BIT %d,%s", bits, idxstr);
        } else if (cbop < 0xC0) {
            snprintf(buf, 64, "RES %d,%s", bits, idxstr);
        } else {
            snprintf(buf, 64, "SET %d,%s", bits, idxstr);
        }
        return 3;
    }

    switch (op) {
    case 0x21: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD %s,#%04X", ir, nn); return 3; }
    case 0x22: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD (#%04X),%s", nn, ir); return 3; }
    case 0x2A: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD %s,(#%04X)", ir, nn); return 3; }
    case 0x23: snprintf(buf, 64, "INC %s", ir); return 1;
    case 0x2B: snprintf(buf, 64, "DEC %s", ir); return 1;
    case 0x09: snprintf(buf, 64, "ADD %s,BC", ir); return 1;
    case 0x19: snprintf(buf, 64, "ADD %s,DE", ir); return 1;
    case 0x29: snprintf(buf, 64, "ADD %s,%s", ir, ir); return 1;
    case 0x39: snprintf(buf, 64, "ADD %s,SP", ir); return 1;
    case 0xE1: snprintf(buf, 64, "POP %s", ir); return 1;
    case 0xE5: snprintf(buf, 64, "PUSH %s", ir); return 1;
    case 0xE9: snprintf(buf, 64, "JP (%s)", ir); return 1;
    case 0xF9: snprintf(buf, 64, "LD SP,%s", ir); return 1;
    case 0xE3: snprintf(buf, 64, "EX (SP),%s", ir); return 1;
    case 0x34: { i8 d = (i8)mem[addr+1]; snprintf(buf, 64, "INC (%s%+d)", ir, d); return 2; }
    case 0x35: { i8 d = (i8)mem[addr+1]; snprintf(buf, 64, "DEC (%s%+d)", ir, d); return 2; }
    case 0x36: { i8 d = (i8)mem[addr+1]; u8 n = mem[addr+2]; snprintf(buf, 64, "LD (%s%+d),#%02X", ir, d, n); return 3; }
    default:
        if ((op & 0xC0) == 0x40) {
            /* LD r,(IX/IY+d) or LD (IX/IY+d),r */
            int dst = (op >> 3) & 7;
            int src = op & 7;
            if (src == 6) { /* LD r,(IX+d) */
                i8 d = (i8)mem[addr+1];
                snprintf(buf, 64, "LD %s,(%s%+d)", r8_names[dst], ir, d);
                return 2;
            } else if (dst == 6) { /* LD (IX+d),r */
                i8 d = (i8)mem[addr+1];
                snprintf(buf, 64, "LD (%s%+d),%s", ir, d, r8_names[src]);
                return 2;
            }
        }
        if ((op & 0xC0) == 0x80 && (op & 7) == 6) {
            /* ALU A,(IX+d) */
            i8 d = (i8)mem[addr+1];
            snprintf(buf, 64, "%s(%s%+d)", alu_names[(op >> 3) & 7], ir, d);
            return 2;
        }
        snprintf(buf, 64, "NOP ; %s prefix", ir);
        return 1;
    }
}

int z80_disasm(const u8 *mem, u16 addr, char *buf, int buf_size) {
    (void)buf_size;
    u8 op = mem[addr];
    int len = 1;

    switch (op) {
    case 0x00: snprintf(buf, 64, "NOP"); break;
    case 0x01: case 0x11: case 0x21: case 0x31: {
        u16 nn = mem[addr+1] | (mem[addr+2] << 8);
        snprintf(buf, 64, "LD %s,#%04X", r16_names[(op >> 4) & 3], nn);
        len = 3; break;
    }
    case 0x02: snprintf(buf, 64, "LD (BC),A"); break;
    case 0x12: snprintf(buf, 64, "LD (DE),A"); break;
    case 0x0A: snprintf(buf, 64, "LD A,(BC)"); break;
    case 0x1A: snprintf(buf, 64, "LD A,(DE)"); break;
    case 0x22: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD (#%04X),HL", nn); len = 3; break; }
    case 0x2A: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD HL,(#%04X)", nn); len = 3; break; }
    case 0x32: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD (#%04X),A", nn); len = 3; break; }
    case 0x3A: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "LD A,(#%04X)", nn); len = 3; break; }
    case 0x03: case 0x13: case 0x23: case 0x33:
        snprintf(buf, 64, "INC %s", r16_names[(op >> 4) & 3]); break;
    case 0x0B: case 0x1B: case 0x2B: case 0x3B:
        snprintf(buf, 64, "DEC %s", r16_names[(op >> 4) & 3]); break;
    case 0x04: case 0x0C: case 0x14: case 0x1C:
    case 0x24: case 0x2C: case 0x34: case 0x3C:
        snprintf(buf, 64, "INC %s", r8_names[(op >> 3) & 7]);
        if (((op >> 3) & 7) == 6) len = 1; break;
    case 0x05: case 0x0D: case 0x15: case 0x1D:
    case 0x25: case 0x2D: case 0x35: case 0x3D:
        snprintf(buf, 64, "DEC %s", r8_names[(op >> 3) & 7]); break;
    case 0x06: case 0x0E: case 0x16: case 0x1E:
    case 0x26: case 0x2E: case 0x36: case 0x3E:
        snprintf(buf, 64, "LD %s,#%02X", r8_names[(op >> 3) & 7], mem[addr+1]);
        len = 2; break;
    case 0x07: snprintf(buf, 64, "RLCA"); break;
    case 0x0F: snprintf(buf, 64, "RRCA"); break;
    case 0x17: snprintf(buf, 64, "RLA"); break;
    case 0x1F: snprintf(buf, 64, "RRA"); break;
    case 0x08: snprintf(buf, 64, "EX AF,AF'"); break;
    case 0x09: case 0x19: case 0x29: case 0x39:
        snprintf(buf, 64, "ADD HL,%s", r16_names[(op >> 4) & 3]); break;
    case 0x10: snprintf(buf, 64, "DJNZ #%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x18: snprintf(buf, 64, "JR #%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x20: snprintf(buf, 64, "JR NZ,#%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x28: snprintf(buf, 64, "JR Z,#%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x30: snprintf(buf, 64, "JR NC,#%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x38: snprintf(buf, 64, "JR C,#%04X", (u16)(addr + 2 + (i8)mem[addr+1])); len = 2; break;
    case 0x27: snprintf(buf, 64, "DAA"); break;
    case 0x2F: snprintf(buf, 64, "CPL"); break;
    case 0x37: snprintf(buf, 64, "SCF"); break;
    case 0x3F: snprintf(buf, 64, "CCF"); break;
    case 0x76: snprintf(buf, 64, "HALT"); break;
    case 0xC9: snprintf(buf, 64, "RET"); break;
    case 0xD9: snprintf(buf, 64, "EXX"); break;
    case 0xE3: snprintf(buf, 64, "EX (SP),HL"); break;
    case 0xE9: snprintf(buf, 64, "JP (HL)"); break;
    case 0xEB: snprintf(buf, 64, "EX DE,HL"); break;
    case 0xF3: snprintf(buf, 64, "DI"); break;
    case 0xFB: snprintf(buf, 64, "EI"); break;
    case 0xF9: snprintf(buf, 64, "LD SP,HL"); break;

    case 0xC3: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "JP #%04X", nn); len = 3; break; }
    case 0xCD: { u16 nn = mem[addr+1] | (mem[addr+2] << 8); snprintf(buf, 64, "CALL #%04X", nn); len = 3; break; }
    case 0xD3: snprintf(buf, 64, "OUT (#%02X),A", mem[addr+1]); len = 2; break;
    case 0xDB: snprintf(buf, 64, "IN A,(#%02X)", mem[addr+1]); len = 2; break;

    case 0xCB: len = 1 + disasm_cb(mem, addr + 1, buf); break;
    case 0xED: len = 1 + disasm_ed(mem, addr + 1, buf); break;
    case 0xDD: len = 1 + disasm_ddfd(mem, addr + 1, buf, "IX"); break;
    case 0xFD: len = 1 + disasm_ddfd(mem, addr + 1, buf, "IY"); break;

    default:
        if (op >= 0x40 && op <= 0x7F) {
            snprintf(buf, 64, "LD %s,%s", r8_names[(op >> 3) & 7], r8_names[op & 7]);
        } else if (op >= 0x80 && op <= 0xBF) {
            snprintf(buf, 64, "%s%s", alu_names[(op >> 3) & 7], r8_names[op & 7]);
        } else if ((op & 0xC7) == 0xC0) {
            snprintf(buf, 64, "RET %s", cc_names[(op >> 3) & 7]);
        } else if ((op & 0xC7) == 0xC2) {
            u16 nn = mem[addr+1] | (mem[addr+2] << 8);
            snprintf(buf, 64, "JP %s,#%04X", cc_names[(op >> 3) & 7], nn); len = 3;
        } else if ((op & 0xC7) == 0xC4) {
            u16 nn = mem[addr+1] | (mem[addr+2] << 8);
            snprintf(buf, 64, "CALL %s,#%04X", cc_names[(op >> 3) & 7], nn); len = 3;
        } else if ((op & 0xCF) == 0xC1) {
            snprintf(buf, 64, "POP %s", r16af_names[(op >> 4) & 3]);
        } else if ((op & 0xCF) == 0xC5) {
            snprintf(buf, 64, "PUSH %s", r16af_names[(op >> 4) & 3]);
        } else if ((op & 0xC7) == 0xC6) {
            snprintf(buf, 64, "%s#%02X", alu_names[(op >> 3) & 7], mem[addr+1]); len = 2;
        } else if ((op & 0xC7) == 0xC7) {
            snprintf(buf, 64, "RST #%02X", op & 0x38);
        } else {
            snprintf(buf, 64, "DB #%02X", op);
        }
        break;
    }

    return len;
}
