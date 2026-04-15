/*
 * SPEmulator — Floppy Disk Controller (WD1793)
 */
#ifndef SPEMU_FDD_H
#define SPEMU_FDD_H

#include "types.h"

typedef struct {
    u8   status;
    u8   command;
    u8   track;
    u8   sector;
    u8   data;
    u8   side;
    bool motor_on;
    bool present;
    void *image_file;
    u64   image_size;
    /* Disk geometry */
    int  tracks;
    int  sides;
    int  sectors_per_track;
    int  sector_size;
} sp_fdd_drive_t;

typedef struct {
    sp_fdd_drive_t drives[2];
    u8 selected_drive;
    u8 system_reg;  /* System/control register */
} sp_fdd_t;

void fdd_init(sp_fdd_t *fdd);
void fdd_destroy(sp_fdd_t *fdd);
int  fdd_attach_image(sp_fdd_t *fdd, int drive, const char *path);
u8   fdd_read(sp_fdd_t *fdd, u16 port);
void fdd_write(sp_fdd_t *fdd, u16 port, u8 data);

#endif
