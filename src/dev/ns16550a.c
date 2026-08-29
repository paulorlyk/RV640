//
// Created by palulukan on 8/4/26.
//

#include "ns16550a.h"

#include "../ui.h"

#include <string.h>

static inline void _irq(const Ns16550a *self) {
  const bool irq = (self->ier & NS16550A_IER_DR && self->ringBuffer.len) || (self->ier & NS16550A_IER_THRE);
  if(irq)
    plic_interrupt(self->plic, NS16550A_IRQ);
}

static inline int _incBufIdx(int idx) {
  return (idx + 1) % NS16550A_BUF_SIZE;
}

static inline char _popChar(Ns16550a *self) {
  if(!self->ringBuffer.len)
    return 0;

  const char res = self->ringBuffer.buf[self->ringBuffer.rdIdx];
  self->ringBuffer.rdIdx = _incBufIdx(self->ringBuffer.rdIdx);
  --self->ringBuffer.len;

  return res;
}

static void _cbRead(void *opaque, cpu_addr_t addr, void* buf, size_t size) {
  Ns16550a *self = opaque;

  uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    switch(addr + i) {
      case 4: // Modem Control Register
      case 6: // Modem Status Register
      case 7: // Scratch Pad Register
      default: {
        reg[i] = 0;
        break;
      }

      case 0: {
        if((self->lcr & NS16550A_LCR_DLAB) == 0) {
          // Receiver Holding Register
          reg[i] = _popChar(self);
        } else {
          // Divisor Latch, Least significant byte
        }
        break;
      }

      case 1: {
        if((self->lcr & NS16550A_LCR_DLAB) == 0) {
          // Interrupt Enable Register
          reg[i] = self->ier;
        } else {
          // Divisor Latch, Most significant byte
        }
        break;
      }

      case 2: {
        // Interrupt Status Register
        uint8_t iic = NS16550A_ISR_IIC_NO;
        if(self->ringBuffer.len && self->ier & NS16550A_IER_DR)
          iic = NS16550A_ISR_IIC_RDR;
        else if(self->ier & NS16550A_IER_THRE)
          iic = NS16550A_ISR_IIC_THRE;  // Always ready to transmit

        reg[i] = ((uint8_t)(iic << NS16550A_ISR_IIC_OFFSET)) | (iic == NS16550A_ISR_IIC_NO ? NS16550A_ISR_IS : 0);
        break;
      }

      case 3: {
        // Line Control Register
        reg[i] = self->lcr;
        break;
      }

      case 5: {
        // Line Status Register
        reg[i] = NS16550A_LSR_TXE | NS16550A_LSR_THRE | (self->ringBuffer.len ? NS16550A_LSR_DR : 0);
        break;
      }
    }
  }
}

static void _cbWrite(void *opaque, cpu_addr_t addr, const void* buf, size_t size) {
  Ns16550a *self = opaque;

  const uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    switch(addr + i) {
      case 2: // FIFO Control Register
      case 4: // Modem Control Register
      case 7: // Scratch Pad Register
      default:
        break;

      case 0: {
        if((self->lcr & NS16550A_LCR_DLAB) == 0) {
          // Transmitter Holding Register
          ui_putch((char)reg[i]);
        } else {
          // Divisor Latch, Least significant byte
        }
        break;
      }

      case 1: {
        if((self->lcr & NS16550A_LCR_DLAB) == 0) {
          // Interrupt Enable Register
          self->ier = reg[i];
          _irq(self);
        } else {
          // Divisor Latch, Most significant byte
        }
        break;
      }

      case 3: {
        // Line Control Register
        self->lcr = reg[i];
        break;
      }
    }
  }
}

bool ns16550a_init(Ns16550a *self, Plic* plic) {
  memset(self, 0, sizeof(*self));

  self->plic = plic;

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
  if(self->ringBuffer.len >= NS16550A_BUF_SIZE)
    return false;

  self->ringBuffer.buf[self->ringBuffer.wrIdx] = ch;
  self->ringBuffer.wrIdx = _incBufIdx(self->ringBuffer.wrIdx);
  ++self->ringBuffer.len;

  _irq(self);

  return true;
}
