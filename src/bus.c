/*
 * SPEmulator — Device Bus
 */
#include "bus.h"
#include <string.h>
#include <stdio.h>

void bus_init(sp_bus_t *bus, sp_machine_t *machine) {
    memset(bus, 0, sizeof(sp_bus_t));
    bus->machine = machine;
}

void bus_destroy(sp_bus_t *bus) {
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].destroy)
            bus->devices[i].destroy(bus->devices[i].ctx);
    }
    bus->device_count = 0;
    bus->port_sub_count = 0;
}

int bus_register_device(sp_bus_t *bus, const sp_device_t *dev) {
    if (bus->device_count >= BUS_MAX_DEVICES) {
        fprintf(stderr, "bus: max devices reached\n");
        return -1;
    }
    bus->devices[bus->device_count++] = *dev;
    return 0;
}

int bus_subscribe_port(sp_bus_t *bus, u16 mask, u16 value,
                       port_read_fn read, port_write_fn write,
                       void *ctx, const char *name) {
    if (bus->port_sub_count >= BUS_MAX_PORT_SUBS) {
        fprintf(stderr, "bus: max port subscriptions reached\n");
        return -1;
    }
    sp_port_sub_t *sub = &bus->port_subs[bus->port_sub_count++];
    sub->mask = mask;
    sub->value = value;
    sub->read = read;
    sub->write = write;
    sub->ctx = ctx;
    sub->name = name;
    return 0;
}

u8 bus_port_read(sp_bus_t *bus, u16 port) {
    for (int i = 0; i < bus->port_sub_count; i++) {
        sp_port_sub_t *sub = &bus->port_subs[i];
        if ((port & sub->mask) == sub->value && sub->read) {
            return sub->read(sub->ctx, port);
        }
    }
    return 0xFF; /* floating bus */
}

void bus_port_write(sp_bus_t *bus, u16 port, u8 data) {
    for (int i = 0; i < bus->port_sub_count; i++) {
        sp_port_sub_t *sub = &bus->port_subs[i];
        if ((port & sub->mask) == sub->value && sub->write) {
            sub->write(sub->ctx, port, data);
            return;
        }
    }
}

void bus_reset_all(sp_bus_t *bus) {
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].reset)
            bus->devices[i].reset(bus->devices[i].ctx);
    }
}

void bus_begin_frame(sp_bus_t *bus) {
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].begin_frame)
            bus->devices[i].begin_frame(bus->devices[i].ctx);
    }
}

void bus_end_frame(sp_bus_t *bus) {
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].end_frame)
            bus->devices[i].end_frame(bus->devices[i].ctx);
    }
}
