//
// Created by palulukan on 7/24/26.
//

#include "bus.h"

#include <stdlib.h>

#include "log.h"

#include <string.h>
#include <ctype.h>

static inline bool _includes(cpu_addr_t base, cpu_size_t size, cpu_addr_t a, cpu_size_t s) {
  return (a >= base) && ((a + (s - 1)) <= (base + size));
}

static inline struct _busDevice *_findDevice(Bus *self, cpu_addr_t addr, size_t size, cpu_addr_t *devAddr) {
  for(int i = 0; i < BUS_MAX_DEVICES; ++i) {
    struct _busDevice *dev = self->devices + i;

    if(dev->dev && _includes(dev->base, dev->dev->size, addr, size)) {
      *devAddr = addr - dev->base;
      return dev;
    }
  }

  return NULL;
}

bool bus_init(Bus* self) {
  memset(self, 0, sizeof(*self));

  return true;
}

void bus_destroy(Bus *self) {
  (void)self;
}

bool bus_register(Bus *self, cpu_addr_t base, Device *dev) {
  int freeDevIdx = 0;
  for(; freeDevIdx < BUS_MAX_DEVICES; ++freeDevIdx) {
    if(!self->devices[freeDevIdx].dev)
      break;
  }
  if(freeDevIdx >= BUS_MAX_DEVICES)
    return false;

  self->devices[freeDevIdx].base = base;
  self->devices[freeDevIdx].dev = dev;

  return true;
}

bool bus_read(Bus *self, cpu_addr_t addr, void* buf, size_t size) {
  cpu_addr_t devAddr;
  struct _busDevice *dev = _findDevice(self, addr, size, &devAddr);
  if(!dev)
    return false;

  dev->dev->cbRead(dev->dev->opaque, devAddr, buf, size);

  return true;
}

bool bus_write(Bus *self, cpu_addr_t addr, const void* buf, size_t size) {
  cpu_addr_t devAddr;
  struct _busDevice *dev = _findDevice(self, addr, size, &devAddr);
  if(!dev)
    return false;

  if(self->reservation.valid && _includes(self->reservation.addr, self->reservation.size, addr, size))
    self->reservation.valid = false;

  dev->dev->cbWrite(dev->dev->opaque, devAddr, buf, size);

  return true;
}

void bus_reservationCreate(Bus *self, cpu_addr_t addr, size_t size) {
  self->reservation.addr = addr;
  self->reservation.size = size;
  self->reservation.valid = true;
}

bool bus_reservationCheckInvalidate(Bus *self, cpu_addr_t addr, size_t size) {
  if(!self->reservation.valid)
    return false;

  self->reservation.valid = false;

  return _includes(self->reservation.addr, self->reservation.size, addr, size);
}

bool bus_dump(Bus *self, cpu_addr_t addr, size_t size) {
  uint8_t *mem = malloc(size);
  if(!mem || !bus_read(self, addr, mem, size)) {
    free(mem);
    return false;
  }

  uint8_t *memPtr = mem;
  for(size_t printed = 0; printed < size;) {
    const size_t lineBytes = 16;

    const size_t lineSize = min_size_t(lineBytes, size - printed);

    char buf[256] = {};
    char *ptr = buf;

    ptr += sprintf(ptr, "%08" PRI_CPU_PTR ": ", addr + (cpu_addr_t)printed);

    for(size_t i = 0; i < lineSize; ++i)
      ptr += sprintf(ptr, "%02x ", memPtr[i]);
    for(size_t i = 0; i < lineBytes - lineSize; ++i)
      ptr += sprintf(ptr, "   ");
    ptr += sprintf(ptr, "| ");
    for(size_t i = 0; i < lineSize; ++i) {
      *ptr = isprint(memPtr[i]) ? memPtr[i] : '.';
      ++ptr;
    }

    DEBUG("%s", buf);

    printed += lineSize;
    memPtr += lineSize;
  }

  free(mem);
  return true;
}
