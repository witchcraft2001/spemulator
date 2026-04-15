/*
 * SPEmulator — Disk Image Backends
 */
#include "disk/image.h"
#include <string.h>

sp_image_type_t image_detect_type(const char *path) {
    if (!path) return IMG_TYPE_UNKNOWN;
    const char *ext = strrchr(path, '.');
    if (!ext) return IMG_TYPE_RAW;

    if (strcasecmp(ext, ".img") == 0) return IMG_TYPE_RAW;
    if (strcasecmp(ext, ".chd") == 0) return IMG_TYPE_CHD;
    if (strcasecmp(ext, ".iso") == 0) return IMG_TYPE_ISO;
    if (strcasecmp(ext, ".bin") == 0) return IMG_TYPE_RAW;

    return IMG_TYPE_UNKNOWN;
}
