/*
 * SPEmulator — Trace/Logging System
 */
#include "debug/trace.h"
#include <stdarg.h>
#include <string.h>

sp_trace_t g_trace;

void trace_init(sp_trace_t *tr) {
    memset(tr, 0, sizeof(*tr));
    tr->out = stderr;
    tr->mask = TR_DEFAULT;
    tr->enabled = false;
}

void trace_parse_spec(sp_trace_t *tr, const char *spec) {
    if (!spec || !spec[0]) {
        tr->mask = TR_DEFAULT;
        return;
    }
    if (strcmp(spec, "all") == 0)     { tr->mask = TR_ALL; return; }
    if (strcmp(spec, "default") == 0) { tr->mask = TR_DEFAULT; return; }
    if (strcmp(spec, "verbose") == 0) { tr->mask = TR_VERBOSE; return; }
    if (strcmp(spec, "io") == 0)      { tr->mask = TR_IO; return; }

    /* Parse comma-separated categories */
    tr->mask = 0;
    char buf[256];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        if (strcmp(tok, "port_r") == 0 || strcmp(tok, "pr") == 0)    tr->mask |= TR_PORT_R;
        else if (strcmp(tok, "port_w") == 0 || strcmp(tok, "pw") == 0) tr->mask |= TR_PORT_W;
        else if (strcmp(tok, "port") == 0 || strcmp(tok, "io") == 0)  tr->mask |= TR_IO;
        else if (strcmp(tok, "mem_r") == 0 || strcmp(tok, "mr") == 0) tr->mask |= TR_MEM_R;
        else if (strcmp(tok, "mem_w") == 0 || strcmp(tok, "mw") == 0) tr->mask |= TR_MEM_W;
        else if (strcmp(tok, "mem") == 0)   tr->mask |= TR_ALL_MEM;
        else if (strcmp(tok, "page") == 0)  tr->mask |= TR_PAGE;
        else if (strcmp(tok, "irq") == 0)   tr->mask |= TR_IRQ;
        else if (strcmp(tok, "boot") == 0)  tr->mask |= TR_BOOT;
        else if (strcmp(tok, "cmos") == 0)  tr->mask |= TR_CMOS;
        else if (strcmp(tok, "cpu") == 0)   tr->mask |= TR_CPU;
        else if (strcmp(tok, "ctc") == 0)   tr->mask |= TR_CTC;
        else if (strcmp(tok, "video") == 0) tr->mask |= TR_VIDEO;
        tok = strtok(NULL, ",");
    }
}

void trace_log(u32 category, u64 tstates, u16 pc, const char *fmt, ...) {
    /* Category label */
    const char *cat;
    switch (category) {
    case TR_PORT_R: cat = "IO"; break;
    case TR_PORT_W: cat = "IO"; break;
    case TR_MEM_R:  cat = "MEM"; break;
    case TR_MEM_W:  cat = "MEM"; break;
    case TR_PAGE:   cat = "PAGE"; break;
    case TR_IRQ:    cat = "IRQ"; break;
    case TR_BOOT:   cat = "BOOT"; break;
    case TR_CMOS:   cat = "CMOS"; break;
    case TR_CPU:    cat = "CPU"; break;
    case TR_CTC:    cat = "CTC"; break;
    case TR_VIDEO:  cat = "VID"; break;
    default:        cat = "???"; break;
    }

    fprintf(g_trace.out, "T=%010llu PC=%04X [%-4s] ",
            (unsigned long long)tstates, (unsigned)pc, cat);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_trace.out, fmt, ap);
    va_end(ap);

    fputc('\n', g_trace.out);
}
