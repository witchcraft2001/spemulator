/*
 * SPEmulator — Debugger Core
 */
#include "debug/debugger.h"
#include "machine.h"
#include "cpu/z80_disasm.h"
#include <stdio.h>
#include <string.h>

void debugger_init(sp_debugger_t *dbg) {
    memset(dbg, 0, sizeof(*dbg));
    bp_init(&dbg->breakpoints);
    dbg->state = DBG_STATE_RUNNING;
}

void debugger_reset(sp_debugger_t *dbg) {
    dbg->state = DBG_STATE_RUNNING;
}

bool debugger_pre_step(sp_debugger_t *dbg, sp_machine_t *m) {
    if (!dbg->active) return true;

    u16 pc = m->cpu.pc.w;

    switch (dbg->state) {
    case DBG_STATE_RUNNING:
        if (bp_check_exec(&dbg->breakpoints, pc)) {
            printf("Breakpoint hit at #%04X\n", pc);
            dbg->state = DBG_STATE_PAUSED;
            debugger_print_regs(dbg, m);
            debugger_print_disasm(dbg, m, pc, 5);
            return false;
        }
        return true;

    case DBG_STATE_STEPPING:
        dbg->state = DBG_STATE_PAUSED;
        debugger_print_regs(dbg, m);
        debugger_print_disasm(dbg, m, pc, 5);
        return false;

    case DBG_STATE_STEP_OVER:
        if (pc == dbg->step_over_addr) {
            dbg->state = DBG_STATE_PAUSED;
            debugger_print_regs(dbg, m);
            debugger_print_disasm(dbg, m, pc, 5);
            return false;
        }
        return true;

    case DBG_STATE_PAUSED:
        return false;
    }

    return true;
}

void debugger_break(sp_debugger_t *dbg) {
    dbg->state = DBG_STATE_PAUSED;
}

void debugger_continue(sp_debugger_t *dbg) {
    dbg->state = DBG_STATE_RUNNING;
}

void debugger_step(sp_debugger_t *dbg) {
    dbg->state = DBG_STATE_STEPPING;
}

void debugger_step_over(sp_debugger_t *dbg, sp_machine_t *m) {
    /* Determine next instruction length */
    u16 pc = m->cpu.pc.w;
    u8 mem_buf[8];
    for (int i = 0; i < 8; i++)
        mem_buf[i] = machine_mem_read(m, (u16)(pc + i));

    char buf[64];
    int len = z80_disasm(mem_buf, 0, buf, sizeof(buf));

    /* Check if current instruction is CALL or RST */
    u8 op = mem_buf[0];
    bool is_call = (op == 0xCD) || ((op & 0xC7) == 0xC4) ||  /* CALL / CALL cc */
                   ((op & 0xC7) == 0xC7);                      /* RST */

    if (is_call) {
        dbg->step_over_addr = pc + len;
        dbg->state = DBG_STATE_STEP_OVER;
    } else {
        dbg->state = DBG_STATE_STEPPING;
    }
}

void debugger_print_regs(sp_debugger_t *dbg, sp_machine_t *m) {
    (void)dbg;
    z80_t *cpu = &m->cpu;
    printf("AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X PC=%04X\n",
           cpu->af.w, cpu->bc.w, cpu->de.w, cpu->hl.w, cpu->sp.w, cpu->pc.w);
    printf("AF'=%04X BC'=%04X DE'=%04X HL'=%04X IX=%04X IY=%04X\n",
           cpu->af2.w, cpu->bc2.w, cpu->de2.w, cpu->hl2.w, cpu->ix.w, cpu->iy.w);
    printf("I=%02X R=%02X IM=%d IFF1=%d IFF2=%d %s\n",
           cpu->i, (cpu->r & 0x7F) | (cpu->r7 & 0x80), cpu->im,
           cpu->iff1, cpu->iff2, cpu->halted ? "HALTED" : "");
    printf("Pages: W0=#%02X W1=#%02X W2=#%02X W3=#%02X  Mode=%d\n",
           m->page_reg[0], m->page_reg[1], m->page_reg[2], m->page_reg[3],
           m->video_mode);
}

void debugger_print_disasm(sp_debugger_t *dbg, sp_machine_t *m, u16 addr, int lines) {
    (void)dbg;
    for (int i = 0; i < lines; i++) {
        u8 mem_buf[8];
        for (int j = 0; j < 8; j++)
            mem_buf[j] = machine_mem_read(m, (u16)(addr + j));

        char buf[64];
        int len = z80_disasm(mem_buf, 0, buf, sizeof(buf));

        /* Print address, hex bytes, mnemonic */
        printf("%04X  ", addr);
        for (int j = 0; j < 4; j++) {
            if (j < len)
                printf("%02X ", mem_buf[j]);
            else
                printf("   ");
        }
        printf(" %s\n", buf);

        addr += len;
    }
}

void debugger_print_memdump(sp_machine_t *m, u8 page, u16 offset, int bytes) {
    u32 base = (u32)page << 14;
    for (int i = 0; i < bytes; i += 16) {
        printf("%02X:%04X  ", page, offset + i);
        for (int j = 0; j < 16 && (i + j) < bytes; j++) {
            u32 addr = base + ((offset + i + j) & 0x3FFF);
            u8 val;
            if (page >= 0x80)
                val = (addr < SP_ROM_SIZE) ? m->rom[addr - (0x80 << 14)] : 0xFF;
            else
                val = (addr < SP_RAM_SIZE) ? m->ram[addr] : 0xFF;
            printf("%02X ", val);
        }
        printf(" ");
        for (int j = 0; j < 16 && (i + j) < bytes; j++) {
            u32 addr = base + ((offset + i + j) & 0x3FFF);
            u8 val;
            if (page >= 0x80)
                val = (addr < SP_ROM_SIZE) ? m->rom[addr - (0x80 << 14)] : 0;
            else
                val = (addr < SP_RAM_SIZE) ? m->ram[addr] : 0;
            printf("%c", (val >= 0x20 && val < 0x7F) ? val : '.');
        }
        printf("\n");
    }
}
