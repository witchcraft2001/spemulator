/*
 * SPEmulator — IDE/ATA Controller
 */
#include "disk/ide.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ATA status bits */
#define ATA_ST_BSY   0x80
#define ATA_ST_RDY   0x40
#define ATA_ST_DRQ   0x08
#define ATA_ST_ERR   0x01

/* ATA commands */
#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30

static sp_ide_device_t *selected(sp_ide_t *ide) {
    return &ide->devices[ide->selected_device];
}

void ide_init(sp_ide_t *ide) {
    memset(ide, 0, sizeof(*ide));
    for (int i = 0; i < 2; i++) {
        ide->devices[i].status = ATA_ST_RDY;
    }
}

void ide_destroy(sp_ide_t *ide) {
    for (int i = 0; i < 2; i++) {
        if (ide->devices[i].image_file) {
            fclose((FILE *)ide->devices[i].image_file);
            ide->devices[i].image_file = NULL;
        }
    }
}

int ide_attach_image(sp_ide_t *ide, int dev_idx, const char *path, bool is_cdrom) {
    if (dev_idx < 0 || dev_idx > 1 || !path || !path[0]) return -1;

    sp_ide_device_t *dev = &ide->devices[dev_idx];
    FILE *f = fopen(path, "r+b");
    if (!f) f = fopen(path, "rb"); /* Try read-only */
    if (!f) {
        fprintf(stderr, "IDE: Cannot open image: %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    dev->image_size = (u64)ftell(f);
    fseek(f, 0, SEEK_SET);

    dev->image_file = f;
    dev->present = true;
    dev->is_cdrom = is_cdrom;
    dev->status = ATA_ST_RDY;

    /* Calculate CHS geometry */
    u64 total_sectors = dev->image_size / IDE_SECTOR_SIZE;
    dev->heads = 16;
    dev->sectors = 63;
    dev->cylinders = (u16)(total_sectors / (dev->heads * dev->sectors));
    if (dev->cylinders > 16383) dev->cylinders = 16383;

    printf("IDE%d: Attached %s (%llu bytes, C/H/S=%d/%d/%d)\n",
           dev_idx, path, (unsigned long long)dev->image_size,
           dev->cylinders, dev->heads, dev->sectors);
    return 0;
}

static u64 get_lba(sp_ide_device_t *dev) {
    if (dev->drive_head & 0x40) {
        /* LBA mode */
        return (u64)dev->sector_num
             | ((u64)dev->cyl_lo << 8)
             | ((u64)dev->cyl_hi << 16)
             | ((u64)(dev->drive_head & 0x0F) << 24);
    } else {
        /* CHS mode */
        u64 c = dev->cyl_lo | (dev->cyl_hi << 8);
        u64 h = dev->drive_head & 0x0F;
        u64 s = dev->sector_num;
        return (c * dev->heads + h) * dev->sectors + (s - 1);
    }
}

static void cmd_identify(sp_ide_device_t *dev) {
    memset(dev->data_buffer, 0, IDE_SECTOR_SIZE);
    u16 *id = (u16 *)dev->data_buffer;

    id[0] = 0x0040;     /* Fixed disk */
    id[1] = dev->cylinders;
    id[3] = dev->heads;
    id[6] = dev->sectors;
    /* Serial number (words 10-19) */
    memcpy(&id[10], "SPEMU-IDE       ", 20);
    /* Firmware revision (words 23-26) */
    memcpy(&id[23], "0001    ", 8);
    /* Model number (words 27-46) */
    memcpy(&id[27], "SPEmulator Virtual Disk             ", 40);
    id[49] = 0x0200;     /* LBA supported */
    /* Total LBA sectors */
    u32 total = (u32)dev->cylinders * dev->heads * dev->sectors;
    id[60] = total & 0xFFFF;
    id[61] = (total >> 16) & 0xFFFF;

    dev->data_pos = 0;
    dev->data_len = IDE_SECTOR_SIZE;
    dev->status = ATA_ST_RDY | ATA_ST_DRQ;
}

static void cmd_read_sectors(sp_ide_device_t *dev) {
    if (!dev->image_file) {
        dev->status = ATA_ST_RDY | ATA_ST_ERR;
        dev->error = 0x04; /* Abort */
        return;
    }

    u64 lba = get_lba(dev);
    u64 offset = lba * IDE_SECTOR_SIZE;

    FILE *f = (FILE *)dev->image_file;
    fseek(f, (long)offset, SEEK_SET);
    size_t n = fread(dev->data_buffer, 1, IDE_SECTOR_SIZE, f);
    if (n < IDE_SECTOR_SIZE)
        memset(dev->data_buffer + n, 0, IDE_SECTOR_SIZE - n);

    dev->data_pos = 0;
    dev->data_len = IDE_SECTOR_SIZE;
    dev->status = ATA_ST_RDY | ATA_ST_DRQ;
}

static void cmd_write_sectors(sp_ide_device_t *dev) {
    /* Prepare to receive data */
    dev->data_pos = 0;
    dev->data_len = IDE_SECTOR_SIZE;
    dev->status = ATA_ST_RDY | ATA_ST_DRQ;
}

static void execute_command(sp_ide_t *ide) {
    sp_ide_device_t *dev = selected(ide);
    if (!dev->present) {
        dev->status = ATA_ST_ERR;
        return;
    }

    switch (dev->command) {
    case ATA_CMD_IDENTIFY:
        cmd_identify(dev);
        break;
    case ATA_CMD_READ_SECTORS:
        cmd_read_sectors(dev);
        break;
    case ATA_CMD_WRITE_SECTORS:
        cmd_write_sectors(dev);
        break;
    default:
        dev->status = ATA_ST_RDY | ATA_ST_ERR;
        dev->error = 0x04; /* Abort */
        break;
    }
}

u8 ide_read(sp_ide_t *ide, u16 port) {
    sp_ide_device_t *dev = selected(ide);
    u16 reg = port & 0xFF;

    switch (reg) {
    case 0x50: /* Data (16-bit, return low byte then high byte) */
        if (dev->data_pos < dev->data_len) {
            u8 val = dev->data_buffer[dev->data_pos++];
            if (dev->data_pos >= dev->data_len) {
                dev->status = ATA_ST_RDY;
            }
            return val;
        }
        return 0;
    case 0x51: return dev->error;
    case 0x52: return dev->sector_count;
    case 0x53: return dev->sector_num;
    case 0x54: return dev->cyl_lo;
    case 0x55: return dev->cyl_hi;
    }

    /* Status (alt or main) */
    if (reg == 0x53 && (port >> 8) == 0x40)
        return dev->status;

    return dev->status;
}

void ide_write(sp_ide_t *ide, u16 port, u8 data) {
    sp_ide_device_t *dev;
    u16 reg = port & 0xFF;

    switch (reg) {
    case 0x50: /* Data write */
        dev = selected(ide);
        if (dev->data_pos < dev->data_len) {
            dev->data_buffer[dev->data_pos++] = data;
            if (dev->data_pos >= dev->data_len && dev->command == ATA_CMD_WRITE_SECTORS) {
                /* Flush to disk */
                u64 lba = get_lba(dev);
                u64 offset = lba * IDE_SECTOR_SIZE;
                FILE *f = (FILE *)dev->image_file;
                if (f) {
                    fseek(f, (long)offset, SEEK_SET);
                    fwrite(dev->data_buffer, 1, IDE_SECTOR_SIZE, f);
                    fflush(f);
                }
                dev->status = ATA_ST_RDY;
            }
        }
        break;
    case 0x51: ide->devices[ide->selected_device].features = data; break;
    case 0x52: ide->devices[ide->selected_device].sector_count = data; break;
    case 0x53: ide->devices[ide->selected_device].sector_num = data; break;
    case 0x54: ide->devices[ide->selected_device].cyl_lo = data; break;
    case 0x55: ide->devices[ide->selected_device].cyl_hi = data; break;
    case 0x56: /* Drive/Head */
        ide->selected_device = (data >> 4) & 1;
        ide->devices[ide->selected_device].drive_head = data;
        break;
    case 0x57: /* Command */
        ide->devices[ide->selected_device].command = data;
        execute_command(ide);
        break;
    }
}
