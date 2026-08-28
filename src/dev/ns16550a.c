//
// Created by palulukan on 8/4/26.
//

#include "ns16550a.h"

#include "../log.h"
#include "../ui.h"

#include <string.h>

static inline int _incBufIdx(int idx) {
  return (idx + 1) % NS16550A_BUF_SIZE;
}

static void _cbRead(void *opaque, cpu_addr_t addr, void* buf, size_t size) {
  Ns16550a *self = opaque;

  // DEBUG("TEST DEV: Read: 0x%" PRI_CPU_PTR " size: %zu", addr, size);

  uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    switch(addr + i) {
      default:
        reg[i] = 0;
        break;

      case 0: {
        // Receiver Holding Register
        reg[i] = 0;
        if(self->size) {
          reg[i] = self->buf[self->rdIdx];
          self->rdIdx = _incBufIdx(self->rdIdx);
          --self->size;
        }
        break;
      }

      case 5: {
        // Line Status Register
        const uint8_t DR = self->size != 0;
        reg[i] =
            (1U << 6)   // Transmitter Empty
          | (1U << 5)   // THR Empty
          | DR;         // Data Ready
        break;
      }
    }
  }
}

static void _cbWrite(void *opaque, cpu_addr_t addr, const void* buf, size_t size) {
  const uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    switch(addr + i) {
      default:
        break;

      case 0: {
        // Transmitter Holding Register
        ui_putch((char)reg[i]);
        break;
      }

      case 1: {
        // Interrupt Enable Register
        break;
      }
    }
  }
}

bool ns16550a_init(Ns16550a *self) {
  memset(self, 0, sizeof(*self));

  self->dev.opaque = self;
  self->dev.size = 8;
  self->dev.cbRead = &_cbRead;
  self->dev.cbWrite = &_cbWrite;

  return true;
}

void ns16550a_destroy(Ns16550a *self) {
  (void)self;
}

bool ns16550_push(Ns16550a *self, char ch) {
  if(self->size >= NS16550A_BUF_SIZE)
    return false;

  self->buf[self->wrIdx] = ch;
  self->wrIdx = _incBufIdx(self->wrIdx);
  ++self->size;

  return true;
}
