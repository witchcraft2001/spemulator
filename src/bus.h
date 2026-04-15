/*
 * SPEmulator — Device Bus
 * Manages device registration, port dispatch, and memory dispatch.
 * Inspired by ZXMAK2's event-based device architecture.
 */
#ifndef SPEMU_BUS_H
#define SPEMU_BUS_H

#include "types.h"

/* Forward declarations */
typedef struct sp_machine sp_machine_t;

/* Device categories for ordering */
typedef enum {
    DEV_CAT_CPU = 0,
    DEV_CAT_MEMORY,
    DEV_CAT_VIDEO,
    DEV_CAT_AUDIO,
    DEV_CAT_DISK,
    DEV_CAT_INPUT,
    DEV_CAT_PERIPH,
    DEV_CAT_DEBUG,
    DEV_CAT_COUNT
} sp_dev_category_t;

/* Port handler callback types */
typedef u8  (*port_read_fn)(void *ctx, u16 port);
typedef void (*port_write_fn)(void *ctx, u16 port, u8 data);

/* Memory handler callback types */
typedef u8  (*mem_read_fn)(void *ctx, u16 addr);
typedef void (*mem_write_fn)(void *ctx, u16 addr, u8 data);

/* Device lifecycle callbacks */
typedef void (*dev_reset_fn)(void *ctx);
typedef void (*dev_frame_fn)(void *ctx);
typedef void (*dev_destroy_fn)(void *ctx);

/* Port subscription: (addr & mask) == value triggers handler */
#define BUS_MAX_PORT_SUBS  128
#define BUS_MAX_DEVICES     32

typedef struct {
    u16          mask;
    u16          value;
    port_read_fn read;
    port_write_fn write;
    void        *ctx;
    const char  *name;  /* device name for debugging */
} sp_port_sub_t;

/* Device descriptor */
typedef struct {
    const char       *name;
    sp_dev_category_t category;
    void             *ctx;
    dev_reset_fn      reset;
    dev_frame_fn      begin_frame;
    dev_frame_fn      end_frame;
    dev_destroy_fn    destroy;
} sp_device_t;

/* Bus state */
typedef struct {
    sp_device_t    devices[BUS_MAX_DEVICES];
    int            device_count;

    sp_port_sub_t  port_subs[BUS_MAX_PORT_SUBS];
    int            port_sub_count;

    sp_machine_t  *machine;
} sp_bus_t;

/* Initialize/destroy bus */
void bus_init(sp_bus_t *bus, sp_machine_t *machine);
void bus_destroy(sp_bus_t *bus);

/* Register a device */
int bus_register_device(sp_bus_t *bus, const sp_device_t *dev);

/* Subscribe to port range: handler called when (port & mask) == value */
int bus_subscribe_port(sp_bus_t *bus, u16 mask, u16 value,
                       port_read_fn read, port_write_fn write,
                       void *ctx, const char *name);

/* Port I/O dispatch */
u8   bus_port_read(sp_bus_t *bus, u16 port);
void bus_port_write(sp_bus_t *bus, u16 port, u8 data);

/* Signal all devices */
void bus_reset_all(sp_bus_t *bus);
void bus_begin_frame(sp_bus_t *bus);
void bus_end_frame(sp_bus_t *bus);

#endif /* SPEMU_BUS_H */
