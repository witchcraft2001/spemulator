/*
 * SPEmulator — Configuration
 * INI file parser and command-line option handling.
 */
#ifndef SPEMU_CONFIG_H
#define SPEMU_CONFIG_H

#include "types.h"

#define SP_MAX_PATH 1024

typedef struct sp_config {
    /* ROM */
    char rom_path[SP_MAX_PATH];

    /* Disk images */
    char fdd0_path[SP_MAX_PATH];
    char fdd1_path[SP_MAX_PATH];
    char ide0_path[SP_MAX_PATH];
    char ide1_path[SP_MAX_PATH];
    char cdrom_path[SP_MAX_PATH];

    /* Video */
    int  scale;           /* 1, 2, 3 */
    bool fullscreen;

    /* CPU */
    int  cpu_speed_mhz;   /* 7 or 21 */
    int  ram_size_kb;      /* 512, 1024, 2048, 4096 */

    /* Audio */
    bool audio_enabled;
    int  sample_rate;

    /* Debug */
    bool start_debugger;
    bool trace_enabled;
    char trace_spec[256];  /* Trace category spec: "all", "io,page", etc. */

    /* Config file path */
    char config_path[SP_MAX_PATH];
} sp_config_t;

/* Create config with defaults */
sp_config_t *config_create(void);
void config_destroy(sp_config_t *cfg);

/* Parse command line args */
int config_parse_args(sp_config_t *cfg, int argc, char **argv);

/* Load INI file */
int config_load_file(sp_config_t *cfg, const char *path);

/* Print usage */
void config_print_usage(const char *argv0);

#endif /* SPEMU_CONFIG_H */
