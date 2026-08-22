//
// Created by palulukan on 8/9/26.
//

#include "plic.h"

#include <string.h>
#include <assert.h>

static void _cbRead(void *opaque, cpu_addr_t addr, void* buf, size_t size) {
  Plic *self = opaque;

  uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    reg[i] = 0;

    if(addr >= sizeof(self->priority[0]) && addr < (sizeof(self->priority) + sizeof(self->priority[0]))) {
      reg[i] = ((uint8_t *)self->priority)[addr - sizeof(self->priority[0])];
    } else if((addr >= 0x1000) && addr < (0x1000 + sizeof(self->pending))) {
      reg[i] = ((uint8_t *)self->pending)[addr - 0x1000];
    } else if((addr >= 0x2000) && addr < 0x200000) {
      const uint32_t a = (uint32_t)addr - 0x2000;
      const uint32_t context = a / 0x80;
      const uint32_t ca = a % 0x80;
      if(context < PLIC_CONTEXTS) {
        if(ca < (sizeof(self->contexts[context].enable))) {
          reg[i] = ((uint8_t *)self->contexts[context].enable)[ca];
        }
      }
    } else if((addr >= 0x200000) && addr < 0x400000) {
      const uint32_t a = (uint32_t)addr - 0x200000;
      const uint32_t context = a / 0x1000;
      const uint32_t ca = a % 0x1000;
      if(context < PLIC_CONTEXTS) {
        if(ca < sizeof(self->contexts[context].threshold)) {
          reg[i] = ((uint8_t *)&self->contexts[context].threshold)[ca];
        } if(ca >= 4 && ca < (4 + sizeof(self->contexts[context].calimComplete))) {
          reg[i] = ((uint8_t *)&self->contexts[context].calimComplete)[ca];
        }
      }
    }

    ++addr;
  }
}

static void _cbWrite(void *opaque, cpu_addr_t addr, const void* buf, size_t size) {
  Plic *self = opaque;

  const uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    if(addr >= sizeof(self->priority[0]) && addr < (sizeof(self->priority) + sizeof(self->priority[0]))) {
      ((uint8_t *)self->priority)[addr - sizeof(self->priority[0])] = reg[i];
    } else if((addr >= 0x1000) && addr < (0x1000 + sizeof(self->pending))) {
      ((uint8_t *)self->pending)[addr - 0x1000] = reg[i];
    } else if((addr >= 0x2000) && addr < 0x200000) {
      const uint32_t a = (uint32_t)addr - 0x2000;
      const uint32_t context = a / 0x80;
      const uint32_t ca = a % 0x80;
      if(context < PLIC_CONTEXTS) {
        if(ca < (sizeof(self->contexts[context].enable))) {
          ((uint8_t *)self->contexts[context].enable)[ca] = reg[i];
        }
      }
    } else if((addr >= 0x200000) && addr < 0x400000) {
      const uint32_t a = (uint32_t)addr - 0x200000;
      const uint32_t context = a / 0x1000;
      const uint32_t ca = a % 0x1000;
      if(context < PLIC_CONTEXTS) {
        if(ca < sizeof(self->contexts[context].threshold)) {
          ((uint8_t *)&self->contexts[context].threshold)[ca] = reg[i];
        } if(ca >= 4 && ca < (4 + sizeof(self->contexts[context].calimComplete))) {
          ((uint8_t *)&self->contexts[context].calimComplete)[ca] = reg[i];
        }
      }
    }

    ++addr;
  }
}

bool plic_init(Plic *self) {
  memset(self, 0, sizeof(*self));

  self->dev.opaque = self;
  self->dev.size = 0x400000UL;
  self->dev.cbRead = &_cbRead;
  self->dev.cbWrite = &_cbWrite;

  return true;
}

void plic_destroy(Plic *self) {
  (void)self;
}
