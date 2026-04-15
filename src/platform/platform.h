/*
 * SPEmulator — Platform Abstraction
 */
#ifndef SPEMU_PLATFORM_H
#define SPEMU_PLATFORM_H

#include "types.h"

/* Get platform name string */
const char *platform_name(void);

/* Get default config file path */
const char *platform_default_config_path(void);

#endif /* SPEMU_PLATFORM_H */
