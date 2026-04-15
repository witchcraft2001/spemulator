/*
 * SPEmulator — Floppy Disk Controller (stub)
 */
#include "disk/fdd.h"
#include <stdio.h>
#include <string.h>

void fdd_init(sp_fdd_t *fdd) {
    memset(fdd, 0, sizeof(*fdd));
    for (int i = 0; i < 2; i++) {
        fdd->drives[i].sector_size = 512;
        fdd->drives[i].sectors_per_track = 9;
        fdd->drives[i].sides = 2;
        fdd->drives[i].tracks = 80;
    }
}

void fdd_destroy(sp_fdd_t *fdd) {
    for (int i = 0; i < 2; i++) {
        if (fdd->drives[i].image_file) {
            fclose((FILE *)fdd->drives[i].image_file);
            fdd->drives[i].image_file = NULL;
        }
    }
}

int fdd_attach_image(sp_fdd_t *fdd, int drive, const char *path) {
    if (drive < 0 || drive > 1 || !path || !path[0]) return -1;

    sp_fdd_drive_t *d = &fdd->drives[drive];
    FILE *f = fopen(path, "r+b");
    if (!f) f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FDD: Cannot open image: %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    d->image_size = (u64)ftell(f);
    fseek(f, 0, SEEK_SET);

    d->image_file = f;
    d->present = true;

    printf("FDD%d: Attached %s (%llu bytes)\n", drive, path, (unsigned long long)d->image_size);
    return 0;
}

u8 fdd_read(sp_fdd_t *fdd, u16 port) {
    (void)port;
    sp_fdd_drive_t *d = &fdd->drives[fdd->selected_drive];
    if (!d->present) return 0xFF;

    /* TODO: Implement WD1793 register reads */
    return d->status;
}

void fdd_write(sp_fdd_t *fdd, u16 port, u8 data) {
    (void)port; (void)data;
    /* TODO: Implement WD1793 register writes and commands */
}
