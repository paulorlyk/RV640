//
// Created by palulukan on 7/26/26.
//

#ifndef DEVICE_H_2517C9DF583A456F9FFB60DB3E26551A
#define DEVICE_H_2517C9DF583A456F9FFB60DB3E26551A

#include "../cpu/rv_types.h"

#include <stddef.h>
#include <stdbool.h>

typedef void (*dev_cb_read)(void *opaque, cpu_addr_t addr, void* buf, size_t size);
typedef void (*dev_cb_write)(void *opaque, cpu_addr_t addr, const void* buf, size_t size);

typedef struct {
  cpu_size_t size;

  void *opaque;

  dev_cb_read cbRead;
  dev_cb_write cbWrite;
} Device;

#endif //DEVICE_H_2517C9DF583A456F9FFB60DB3E26551A
