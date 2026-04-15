/*
 * SPEmulator — DS12887 RTC / CMOS
 *
 * Dallas DS12887 compatible real-time clock with 128 bytes CMOS RAM.
 * Registers 0x00-0x09: RTC (BCD format by default)
 * Registers 0x0A-0x0D: Status registers
 * Registers 0x0E-0x3F: BIOS configuration
 */
#ifndef SPEMU_RTC_H
#define SPEMU_RTC_H

#include "types.h"

/* DS12887 status register A */
#define RTC_REG_A     0x0A
#define RTC_UIP       0x80  /* Update In Progress (bit 7) */
#define RTC_DV_MASK   0x70  /* Divider select bits */
#define RTC_RS_MASK   0x0F  /* Rate select bits */

/* DS12887 status register B */
#define RTC_REG_B     0x0B
#define RTC_SET       0x80  /* SET mode (halts updates) */
#define RTC_PIE       0x40  /* Periodic Interrupt Enable */
#define RTC_AIE       0x20  /* Alarm Interrupt Enable */
#define RTC_UIE       0x10  /* Update-ended Interrupt Enable */
#define RTC_SQWE      0x08  /* Square Wave Enable */
#define RTC_DM        0x04  /* Data Mode: 1=binary, 0=BCD */
#define RTC_24H       0x02  /* 24/12 hour select: 1=24h */
#define RTC_DSE       0x01  /* Daylight Savings Enable */

/* DS12887 status register C */
#define RTC_REG_C     0x0C
/* DS12887 status register D */
#define RTC_REG_D     0x0D
#define RTC_VRT       0x80  /* Valid RAM and Time (battery OK) */

typedef struct {
    u8  cmos[256];       /* CMOS RAM (128 used by DS12887) */
    u8  addr_reg;        /* Address register */
    u32 update_counter;  /* Counter for UIP simulation */
} sp_rtc_t;

void rtc_init(sp_rtc_t *rtc);
void rtc_reset(sp_rtc_t *rtc);
u8   rtc_read(sp_rtc_t *rtc);
void rtc_write_addr(sp_rtc_t *rtc, u8 addr);
void rtc_write_data(sp_rtc_t *rtc, u8 data);

/* Call periodically to advance UIP counter */
void rtc_tick(sp_rtc_t *rtc);

#endif
