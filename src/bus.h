//
// Created by palulukan on 7/24/26.
//

#ifndef BUS_H_0850D3B6569F4E7090F9D9D4188FB66A
#define BUS_H_0850D3B6569F4E7090F9D9D4188FB66A

#include "dev/device.h"

#include <stdbool.h>

#define BUS_MAX_DEVICES 8

typedef struct {
  struct _busDevice {
    Device *dev;
    cpu_addr_t base;
  } devices[BUS_MAX_DEVICES];
  struct {
    cpu_addr_t addr;
    size_t size;
    bool valid;
  } reservation;
} Bus;

bool bus_init(Bus* self);
void bus_destroy(Bus *self);

bool bus_register(Bus *self, cpu_addr_t base, Device *dev);

bool bus_read(Bus *self, cpu_addr_t addr, void* buf, size_t size);
bool bus_write(Bus *self, cpu_addr_t addr, const void* buf, size_t size);

void bus_reservationCreate(Bus *self, cpu_addr_t addr, size_t size);
bool bus_reservationCheckInvalidate(Bus *self, cpu_addr_t addr, size_t size);

bool bus_dump(Bus *self, cpu_addr_t addr, size_t size);

#endif //BUS_H_0850D3B6569F4E7090F9D9D4188FB66A
