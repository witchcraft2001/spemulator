/*
 * SPEmulator — Configuration
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sp_config_t *config_create(void) {
    sp_config_t *cfg = calloc(1, sizeof(sp_config_t));
    if (!cfg) return NULL;

    /* Defaults */
    cfg->scale = 2;
    cfg->fullscreen = false;
    cfg->cpu_speed_mhz = 7;
    cfg->ram_size_kb = 4096;
    cfg->audio_enabled = true;
    cfg->sample_rate = 44100;
    cfg->start_debugger = false;

    return cfg;
}

void config_destroy(sp_config_t *cfg) {
    free(cfg);
}

void config_print_usage(const char *argv0) {
    fprintf(stderr,
        "SPEmulator — Sprinter SP2000 Emulator\n"
        "Usage: %s [options]\n\n"
        "Options:\n"
        "  --rom <file>       BIOS ROM image\n"
        "  --fdd0 <file>      Floppy drive A image\n"
        "  --fdd1 <file>      Floppy drive B image\n"
        "  --ide0 <file>      IDE master (HDD) image\n"
        "  --ide1 <file>      IDE slave image\n"
        "  --cdrom <file>     CD-ROM ISO image\n"
        "  --config <file>    Configuration file path\n"
        "  --scale <1|2|3>    Window scale factor\n"
        "  --fullscreen       Start in fullscreen\n"
        "  --debug            Start with debugger open\n"
        "  --turbo            Start in turbo mode (21MHz)\n"
        "  --help             Show this help\n",
        argv0);
}

static void set_path(char *dst, const char *src) {
    strncpy(dst, src, SP_MAX_PATH - 1);
    dst[SP_MAX_PATH - 1] = '\0';
}

int config_parse_args(sp_config_t *cfg, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            config_print_usage(argv[0]);
            return -1;
        }
        else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
            set_path(cfg->rom_path, argv[++i]);
        else if (strcmp(argv[i], "--fdd0") == 0 && i + 1 < argc)
            set_path(cfg->fdd0_path, argv[++i]);
        else if (strcmp(argv[i], "--fdd1") == 0 && i + 1 < argc)
            set_path(cfg->fdd1_path, argv[++i]);
        else if (strcmp(argv[i], "--ide0") == 0 && i + 1 < argc)
            set_path(cfg->ide0_path, argv[++i]);
        else if (strcmp(argv[i], "--ide1") == 0 && i + 1 < argc)
            set_path(cfg->ide1_path, argv[++i]);
        else if (strcmp(argv[i], "--cdrom") == 0 && i + 1 < argc)
            set_path(cfg->cdrom_path, argv[++i]);
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            set_path(cfg->config_path, argv[++i]);
        else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            cfg->scale = atoi(argv[++i]);
            if (cfg->scale < 1 || cfg->scale > 4) cfg->scale = 2;
        }
        else if (strcmp(argv[i], "--fullscreen") == 0)
            cfg->fullscreen = true;
        else if (strcmp(argv[i], "--debug") == 0)
            cfg->start_debugger = true;
        else if (strcmp(argv[i], "--turbo") == 0)
            cfg->cpu_speed_mhz = 21;
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            config_print_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* Simple INI parser */
static void trim(char *s) {
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ' || s[len-1] == '\t'))
        s[--len] = '\0';
}

int config_load_file(sp_config_t *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#' || line[0] == '[')
            continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        if (strcmp(key, "bios") == 0 || strcmp(key, "rom") == 0)
            set_path(cfg->rom_path, val);
        else if (strcmp(key, "fdd0") == 0)
            set_path(cfg->fdd0_path, val);
        else if (strcmp(key, "fdd1") == 0)
            set_path(cfg->fdd1_path, val);
        else if (strcmp(key, "ide0") == 0)
            set_path(cfg->ide0_path, val);
        else if (strcmp(key, "ide1") == 0)
            set_path(cfg->ide1_path, val);
        else if (strcmp(key, "cdrom") == 0)
            set_path(cfg->cdrom_path, val);
        else if (strcmp(key, "scale") == 0)
            cfg->scale = atoi(val);
        else if (strcmp(key, "fullscreen") == 0)
            cfg->fullscreen = atoi(val) != 0;
        else if (strcmp(key, "cpu_speed") == 0)
            cfg->cpu_speed_mhz = atoi(val);
        else if (strcmp(key, "ram_size") == 0)
            cfg->ram_size_kb = atoi(val);
        else if (strcmp(key, "audio_enabled") == 0 || strcmp(key, "audio") == 0)
            cfg->audio_enabled = atoi(val) != 0;
        else if (strcmp(key, "sample_rate") == 0)
            cfg->sample_rate = atoi(val);
        else if (strcmp(key, "debugger") == 0)
            cfg->start_debugger = atoi(val) != 0;
    }

    fclose(f);
    return 0;
}
