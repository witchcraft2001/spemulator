/*
 * SPEmulator — Disk Image Backends
 * Supports: .img (raw), .chd (MAME compressed), .iso
 */
#ifndef SPEMU_IMAGE_H
#define SPEMU_IMAGE_H

#include "types.h"

typedef enum {
    IMG_TYPE_RAW,
    IMG_TYPE_CHD,
    IMG_TYPE_ISO,
    IMG_TYPE_UNKNOWN
} sp_image_type_t;

/* Detect image type from file extension */
sp_image_type_t image_detect_type(const char *path);

#endif
