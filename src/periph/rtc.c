/*
 * SPEmulator — DS12885 RTC / CMOS
 */
#include "periph/rtc.h"
#include <string.h>
#include <time.h>

void rtc_init(sp_rtc_t *rtc) {
    memset(rtc, 0, sizeof(*rtc));
    /* Set some default CMOS values */
    rtc->cmos[0x0E] = 0x00; /* Flags */
    rtc->cmos[0x10] = 0x02; /* Boot device: IDE1 */
    rtc->cmos[0x11] = 0x01; /* FDD/IDE config */
    rtc->cmos[0x1B] = 0x00; /* Hardware config */
}

void rtc_reset(sp_rtc_t *rtc) {
    rtc->addr_reg = 0;
}

static void update_time(sp_rtc_t *rtc) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t) return;
    rtc->cmos[0x00] = (u8)t->tm_sec;
    rtc->cmos[0x02] = (u8)t->tm_min;
    rtc->cmos[0x04] = (u8)t->tm_hour;
    rtc->cmos[0x06] = (u8)(t->tm_wday + 1);
    rtc->cmos[0x07] = (u8)t->tm_mday;
    rtc->cmos[0x08] = (u8)(t->tm_mon + 1);
    rtc->cmos[0x09] = (u8)(t->tm_year % 100);
}

u8 rtc_read(sp_rtc_t *rtc) {
    if (rtc->addr_reg < 0x0A)
        update_time(rtc);
    return rtc->cmos[rtc->addr_reg];
}

void rtc_write_addr(sp_rtc_t *rtc, u8 addr) {
    rtc->addr_reg = addr;
}

void rtc_write_data(sp_rtc_t *rtc, u8 data) {
    /* Don't allow writing to time registers */
    if (rtc->addr_reg >= 0x0A)
        rtc->cmos[rtc->addr_reg] = data;
}
