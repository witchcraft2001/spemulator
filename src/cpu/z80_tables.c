/*
 * SPEmulator — Z80 Flag/Parity/Timing Tables
 */
#include "z80.h"

u8 z80_sz53_table[256];
u8 z80_parity_table[256];
u8 z80_sz53p_table[256];

void z80_init_tables(void) {
    for (int i = 0; i < 256; i++) {
        /* Sign, Zero, bits 5 and 3 from value */
        u8 sz53 = (u8)(i & (Z80_FLAG_S | Z80_FLAG_5 | Z80_FLAG_3));
        if (i == 0) sz53 |= Z80_FLAG_Z;
        z80_sz53_table[i] = sz53;

        /* Parity: even number of bits = PV set */
        int bits = 0;
        u8 v = (u8)i;
        for (int b = 0; b < 8; b++) {
            bits += v & 1;
            v >>= 1;
        }
        z80_parity_table[i] = (bits & 1) ? 0 : Z80_FLAG_PV;

        /* Combined SZ53P */
        z80_sz53p_table[i] = sz53 | z80_parity_table[i];
    }
}
