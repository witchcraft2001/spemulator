/*
 * SPEmulator — DS12885 RTC / CMOS
 */
#ifndef SPEMU_RTC_H
#define SPEMU_RTC_H

#include "types.h"

typedef struct {
    u8  cmos[256];       /* CMOS RAM */
    u8  addr_reg;        /* Address register */
} sp_rtc_t;

void rtc_init(sp_rtc_t *rtc);
void rtc_reset(sp_rtc_t *rtc);
u8   rtc_read(sp_rtc_t *rtc);
void rtc_write_addr(sp_rtc_t *rtc, u8 addr);
void rtc_write_data(sp_rtc_t *rtc, u8 data);

#endif
