/*
 * SPEmulator — Z80 Disassembler
 */
#ifndef SPEMU_Z80_DISASM_H
#define SPEMU_Z80_DISASM_H

#include "types.h"

/* Disassemble one instruction at `addr`.
 * Writes mnemonic to `buf` (at least 64 bytes).
 * Returns number of bytes consumed by the instruction. */
int z80_disasm(const u8 *mem, u16 addr, char *buf, int buf_size);

#endif
