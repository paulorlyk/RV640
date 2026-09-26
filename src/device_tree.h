//
// Created by palulukan on 9/26/26.
//

#ifndef DEVICE_TREE_H_58BDEB11A5644B059DF90E013DECC42A
#define DEVICE_TREE_H_58BDEB11A5644B059DF90E013DECC42A

#include "dev/device.h"

typedef struct {
  uint8_t *buf;

  uint32_t nextPhandle;
  bool inSOC;

  uint32_t uart0Phandle;
  uint32_t intcPhandle;
  uint32_t plicPhandle;

  cpu_addr_t memAddr;
  cpu_size_t memSize;
} DeviceTree;

bool dt_init(DeviceTree *self);

void dt_destroy(DeviceTree *self);

bool dt_addMemeory(DeviceTree *self, cpu_addr_t addr, cpu_size_t size);
bool dt_addAclint(DeviceTree *self, cpu_addr_t addr, cpu_size_t size);
bool dt_addPlic(DeviceTree *self, cpu_addr_t addr, cpu_size_t size, int irqs);
bool dt_addNs16550a(DeviceTree *self, cpu_addr_t addr, cpu_size_t size, int irq);

bool dt_finish(DeviceTree *self);

static inline const uint8_t *dt_data(DeviceTree *self) { return self->buf; }
size_t dt_size(const DeviceTree *self);

#endif //DEVICE_TREE_H_58BDEB11A5644B059DF90E013DECC42A
