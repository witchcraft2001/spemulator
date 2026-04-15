/*
 * SPEmulator — DS12887 RTC / CMOS
 *
 * Emulates Dallas DS12887 compatible real-time clock.
 * Time stored in BCD format (default mode, register B bit 2 = 0).
 * Status register A bit 7 (UIP) toggles periodically.
 */
#include "periph/rtc.h"
#include <string.h>
#include <time.h>

static u8 bin2bcd(u8 val) {
    return (u8)(((val / 10) << 4) | (val % 10));
}

void rtc_init(sp_rtc_t *rtc) {
    memset(rtc, 0, sizeof(*rtc));

    /* DS12887 status register defaults */
    rtc->cmos[RTC_REG_A] = 0x26;  /* DV=010 (normal), RS=0110 (1024Hz) */
    rtc->cmos[RTC_REG_B] = 0x02;  /* 24-hour mode, BCD format */
    rtc->cmos[RTC_REG_C] = 0x00;
    rtc->cmos[RTC_REG_D] = RTC_VRT; /* Battery OK */

    /* BIOS configuration defaults */
    rtc->cmos[0x0E] = 0x80;  /* Fast boot (skip RAM test) */
    rtc->cmos[0x0F] = 0x10;  /* Keyboard delay */
    rtc->cmos[0x10] = 0x00;  /* Boot device: FDD1 */
    rtc->cmos[0x11] = 0x01;  /* FDD/IDE config */
    rtc->cmos[0x1B] = 0x00;  /* Hardware: normal speed */
    rtc->cmos[0x32] = 0x20;  /* Century: 20 */
}

void rtc_reset(sp_rtc_t *rtc) {
    rtc->addr_reg = 0;
    rtc->update_counter = 0;
}

static void update_time_regs(sp_rtc_t *rtc) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;

    bool bcd = !(rtc->cmos[RTC_REG_B] & RTC_DM);

    u8 sec  = (u8)t->tm_sec;
    u8 min  = (u8)t->tm_min;
    u8 hour = (u8)t->tm_hour;
    u8 wday = (u8)(t->tm_wday == 0 ? 7 : t->tm_wday); /* 1=Mon..7=Sun */
    u8 mday = (u8)t->tm_mday;
    u8 mon  = (u8)(t->tm_mon + 1);
    u8 year = (u8)(t->tm_year % 100);
    u8 cent = (u8)(20);  /* 21st century */

    if (bcd) {
        rtc->cmos[0x00] = bin2bcd(sec);
        rtc->cmos[0x02] = bin2bcd(min);
        rtc->cmos[0x04] = bin2bcd(hour);
        rtc->cmos[0x06] = bin2bcd(wday);
        rtc->cmos[0x07] = bin2bcd(mday);
        rtc->cmos[0x08] = bin2bcd(mon);
        rtc->cmos[0x09] = bin2bcd(year);
        rtc->cmos[0x32] = bin2bcd(cent);
    } else {
        rtc->cmos[0x00] = sec;
        rtc->cmos[0x02] = min;
        rtc->cmos[0x04] = hour;
        rtc->cmos[0x06] = wday;
        rtc->cmos[0x07] = mday;
        rtc->cmos[0x08] = mon;
        rtc->cmos[0x09] = year;
        rtc->cmos[0x32] = cent;
    }

    /* Alarm registers default to "don't care" (0xFF = match any) */
    /* Leave them as-is unless explicitly set */
}

void rtc_tick(sp_rtc_t *rtc) {
    /* Simulate UIP bit: set for ~244us every second.
     * We approximate by toggling every N ticks. */
    rtc->update_counter++;
    /* UIP is clear most of the time, set briefly before update */
    if (rtc->update_counter >= 50) {
        rtc->update_counter = 0;
    }
    /* UIP is set for the last 2 ticks of each 50-tick cycle */
    if (rtc->update_counter >= 48) {
        rtc->cmos[RTC_REG_A] |= RTC_UIP;
    } else {
        rtc->cmos[RTC_REG_A] &= ~RTC_UIP;
    }
}

u8 rtc_read(sp_rtc_t *rtc) {
    u8 addr = rtc->addr_reg;

    /* Time registers: update from host clock */
    if (addr <= 0x09 && !(rtc->cmos[RTC_REG_A] & RTC_UIP)) {
        update_time_regs(rtc);
    }

    /* Register C: reading clears interrupt flags */
    if (addr == RTC_REG_C) {
        u8 val = rtc->cmos[RTC_REG_C];
        rtc->cmos[RTC_REG_C] = 0;
        return val;
    }

    /* Register D: always reports VRT (battery OK) */
    if (addr == RTC_REG_D) {
        return RTC_VRT;
    }

    return rtc->cmos[addr];
}

void rtc_write_addr(sp_rtc_t *rtc, u8 addr) {
    rtc->addr_reg = addr;
}

void rtc_write_data(sp_rtc_t *rtc, u8 data) {
    u8 addr = rtc->addr_reg;

    /* Protect time registers from casual writes (only when SET bit is active) */
    if (addr <= 0x09) {
        if (rtc->cmos[RTC_REG_B] & RTC_SET)
            rtc->cmos[addr] = data;
        return;
    }

    /* Status register A: only RS and DV bits writable */
    if (addr == RTC_REG_A) {
        rtc->cmos[addr] = (rtc->cmos[addr] & RTC_UIP) | (data & ~RTC_UIP);
        return;
    }

    /* Register C, D: read-only */
    if (addr == RTC_REG_C || addr == RTC_REG_D)
        return;

    /* All other registers (including BIOS config) are writable */
    rtc->cmos[addr] = data;
}
