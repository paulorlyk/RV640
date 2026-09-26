//
// Created by palulukan on 9/26/26.
//

#include "device_tree.h"

#include "log.h"

#include "cpu/rv.h"
#include "dev/aclint.h"

#include <stdlib.h>

#include <libfdt.h>

#ifdef CONFIG_RV64

#define BASE_ISA  "rv64i"
#define X_CELLS 2

#else

#define BASE_ISA  "rv32i"
#define X_CELLS 1

#endif

#define FDT_CHECKED_RET_FALSE(expr)               \
  do {                                            \
    int res = (expr);                             \
    if(res != 0) {                                \
      ERROR("FDT error: %s", fdt_strerror(res));  \
      return false;                               \
    }                                             \
  } while(0)

static inline uint32_t _nextPhandle(DeviceTree *self) {
  return ++self->nextPhandle;
}

static inline int _beginAddressedNode(void *fdt, const char* type, cpu_addr_t addr) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s@%" PRI_CPU_PTR, type, addr);

  return fdt_begin_node(fdt, buf);
}

static inline int _writeReg(void *fdt, cpu_addr_t addr, cpu_size_t size) {
#ifdef CONFIG_RV64

#ifdef CONFIG_RV64_32BIT_ADDR

  const uint32_t reg[] = {
    cpu_to_fdt32(0),
    cpu_to_fdt32(addr),
    cpu_to_fdt32(0),
    cpu_to_fdt32(size),
  };

#else

  const uint32_t reg[] = {
    cpu_to_fdt32(addr >> 32),
    cpu_to_fdt32(addr),
    cpu_to_fdt32(size >> 32),
    cpu_to_fdt32(size),
  };

#endif

#else

  const uint32_t reg[] = {
    cpu_to_fdt32(addr),
    cpu_to_fdt32(size),
  };

#endif

  return fdt_property(fdt, "reg", reg, sizeof(reg));
}

bool dt_init(DeviceTree *self) {
  memset(self, 0, sizeof(*self));

  const int bufsize = 2048;

  self->buf = malloc(bufsize);
  if(!self->buf) {
    ERROR("Failed to allocate DTB buffer");
    return false;
  }

  FDT_CHECKED_RET_FALSE(fdt_create(self->buf, bufsize));

  FDT_CHECKED_RET_FALSE(fdt_finish_reservemap(self->buf));

  FDT_CHECKED_RET_FALSE(fdt_begin_node(self->buf, ""));  // /
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#address-cells", X_CELLS));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#size-cells", X_CELLS));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "rv640"));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "model", "RV640"));

  FDT_CHECKED_RET_FALSE(fdt_begin_node(self->buf, "cpus"));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#address-cells", 1));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#size-cells", 0));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "timebase-frequency", ACLINT_FREQENCY));

  FDT_CHECKED_RET_FALSE(_beginAddressedNode(self->buf, "cpu", 0));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "device_type", "cpu"));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "reg", 0));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "status", "okay"));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "riscv"));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "i-cache-block-size", ICACHE_LINE_SIZE));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "i-cache-sets", 1));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "riscv,isa-base", BASE_ISA));
  static const char isaExtensions[] = "i\0" "m\0" "a\0" "c\0" "zaamo\0" "zicboz\0" "zifencei\0" "zicsr\0" "zabha\0";
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "riscv,isa-extensions", isaExtensions, sizeof(isaExtensions) - 1));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "riscv,cboz-block-size", DCACHE_LINE_SIZE));

  FDT_CHECKED_RET_FALSE(fdt_begin_node(self->buf, "interrupt-controller"));
  self->intcPhandle = _nextPhandle(self);
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "phandle", self->intcPhandle));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#interrupt-cells", 1));
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "interrupt-controller", NULL, 0));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "riscv,cpu-intc"));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // interrupt-controller

  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // cpu

  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // cpus

  FDT_CHECKED_RET_FALSE(fdt_begin_node(self->buf, "soc"));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#address-cells", X_CELLS));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#size-cells", X_CELLS));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "simple-bus"));
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "ranges", NULL, 0));

  self->inSOC = true;

  return true;
}

void dt_destroy(DeviceTree *self) {
  free(self->buf);
  self->buf = NULL;
}

bool dt_addMemeory(DeviceTree *self, cpu_addr_t addr, cpu_size_t size) {
  if(!self->inSOC)
    return false;

  self->memAddr = addr;
  self->memSize = size;

  return true;
}

bool dt_addAclint(DeviceTree *self, cpu_addr_t addr, cpu_size_t size) {
  if(!self->inSOC)
    return false;

  FDT_CHECKED_RET_FALSE(_beginAddressedNode(self->buf, "clint", addr));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "riscv,clint0"));
  FDT_CHECKED_RET_FALSE(_writeReg(self->buf, addr, size));
  const uint32_t interruptsExtended[] = {
    cpu_to_fdt32(self->intcPhandle), cpu_to_fdt32(3),
    cpu_to_fdt32(self->intcPhandle), cpu_to_fdt32(7),
  };
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "interrupts-extended", interruptsExtended, sizeof(interruptsExtended)));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // clint

  return true;
}

bool dt_addPlic(DeviceTree *self, cpu_addr_t addr, cpu_size_t size, int irqs) {
  if(!self->inSOC)
    return false;

  FDT_CHECKED_RET_FALSE(_beginAddressedNode(self->buf, "interrupt-controller", addr));
  self->plicPhandle = _nextPhandle(self);
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "phandle", self->plicPhandle));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "riscv,ndev", 1));
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "interrupt-controller", NULL, 0));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#interrupt-cells", irqs));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "#address-cells", 0));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "sifive,plic-1.0.0"));
  FDT_CHECKED_RET_FALSE(_writeReg(self->buf, addr, size));
  const uint32_t interruptsExtended[] = {
    cpu_to_fdt32(self->intcPhandle), cpu_to_fdt32(11),
    cpu_to_fdt32(self->intcPhandle), cpu_to_fdt32(9),
  };
  FDT_CHECKED_RET_FALSE(fdt_property(self->buf, "interrupts-extended", interruptsExtended, sizeof(interruptsExtended)));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // interrupt-controller

  return true;
}

bool dt_addNs16550a(DeviceTree *self, cpu_addr_t addr, cpu_size_t size, int irq) {
  if(!self->inSOC)
    return false;

  FDT_CHECKED_RET_FALSE(_beginAddressedNode(self->buf, "serial", addr));
  if(!self->uart0Phandle) {
    self->uart0Phandle = _nextPhandle(self);
    FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "phandle", self->uart0Phandle));
  }
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "compatible", "ns16550a"));
  if(irq && self->plicPhandle) {
    FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "interrupts", irq));
    FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "interrupt-parent", self->plicPhandle));
  }
  FDT_CHECKED_RET_FALSE(_writeReg(self->buf, addr, size));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "reg-shift", 0));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "reg-io-width", 1));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "clock-frequency", 1843200));
  FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "current-speed", 115200));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "status", "okay"));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // serial

  return true;
}

bool dt_finish(DeviceTree *self) {
  if(!self->inSOC)
    return false;

  self->inSOC = false;

  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // soc

  FDT_CHECKED_RET_FALSE(_beginAddressedNode(self->buf, "memory", self->memAddr));
  FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "device_type", "memory"));
  FDT_CHECKED_RET_FALSE(_writeReg(self->buf, self->memAddr, self->memSize));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // memory

  FDT_CHECKED_RET_FALSE(fdt_begin_node(self->buf, "chosen"));
  if(self->uart0Phandle)
    FDT_CHECKED_RET_FALSE(fdt_property_u32(self->buf, "stdout-path", self->uart0Phandle));
  else
    FDT_CHECKED_RET_FALSE(fdt_property_string(self->buf, "bootargs", "console=ttyS0,115200"));
  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // chosen

  FDT_CHECKED_RET_FALSE(fdt_end_node(self->buf));  // /

  FDT_CHECKED_RET_FALSE(fdt_finish(self->buf));

  return true;
}

size_t dt_size(const DeviceTree *self) {
  return fdt_totalsize(self->buf);
}
