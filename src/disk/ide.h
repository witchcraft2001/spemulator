/*
 * SPEmulator — IDE/ATA Controller
 */
#ifndef SPEMU_IDE_H
#define SPEMU_IDE_H

#include "types.h"

#define IDE_SECTOR_SIZE 512

typedef struct {
    /* Registers */
    u8  error;
    u8  features;
    u8  sector_count;
    u8  sector_num;
    u8  cyl_lo;
    u8  cyl_hi;
    u8  drive_head;
    u8  status;
    u8  command;
    /* Data buffer */
    u8  data_buffer[IDE_SECTOR_SIZE];
    int data_pos;
    int data_len;
    /* Disk image */
    void *image_file;   /* FILE* */
    u64   image_size;
    bool  present;
    bool  is_cdrom;
    /* Geometry */
    u16 cylinders;
    u16 heads;
    u16 sectors;
} sp_ide_device_t;

typedef struct {
    sp_ide_device_t devices[2]; /* master + slave */
    u8 selected_device;         /* 0 or 1 */
} sp_ide_t;

void ide_init(sp_ide_t *ide);
void ide_destroy(sp_ide_t *ide);
int  ide_attach_image(sp_ide_t *ide, int dev_idx, const char *path, bool is_cdrom);
u8   ide_read(sp_ide_t *ide, u16 port);
void ide_write(sp_ide_t *ide, u16 port, u8 data);

#endif
