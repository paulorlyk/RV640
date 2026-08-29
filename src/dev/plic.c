//
// Created by palulukan on 8/9/26.
//

#include "plic.h"

#include <string.h>

static void _processInterrupts(Plic *self) {
  unsigned int irq = 0;
  unsigned int irqWord = 0;
  uint32_t irqMask = 0;
  uint32_t priority = 0;

  // IRQ 0 is reserved
  for(int i = 1; i <= PLIC_INTERRUPTS; ++i) {
    const unsigned int word = i / sizeof(uint32_t);
    const uint32_t mask = (uint32_t)1 << (i % sizeof(uint32_t));

    if((self->pending[word] & mask) && (self->priority[i - 1] > priority)) {
      irq = i;
      irqWord = word;
      irqMask = mask;
      priority = self->priority[i - 1];
    }
  }

  for(int i = 0; i < PLIC_CONTEXTS; ++i) {
    const int hart = i / PLIC_CONTEXTS_PER_HART;
    const RV64_MCAUSE cause = i % PLIC_CONTEXTS_PER_HART ? MCAUSE_SUPERVISOR_EXT_INT : MCAUSE_MACHINE_EXT_INT;

    if(priority > self->contexts[i].threshold && (self->contexts[i].pendingClaims[irqWord] & irqMask) == 0 && self->contexts[i].enable[irqWord] & irqMask) {
      self->contexts[i].claim = irq;

      rv64_setInterrupt(self->harts[hart], cause);
    } else {
      self->contexts[i].claim = 0;

      rv64_clearInterrupt(self->harts[hart], cause);
    }
  }
}

static inline void _claimInterrupts(Plic *self, int context) {
  if(context < 0)
    return;

  const uint32_t claim = self->contexts[context].claim;

  const unsigned int word = claim / sizeof(uint32_t);
  const uint32_t mask = (uint32_t)1 << (claim % sizeof(uint32_t));

  self->contexts[context].pendingClaims[word] |= mask;

  self->pending[word] &= ~mask;

  _processInterrupts(self);
}

static inline void _completeInterrupts(Plic *self, int context, uint32_t irq) {
  if(context < 0) {
    _processInterrupts(self);
    return;
  }

  const unsigned int word = irq / sizeof(uint32_t);
  const uint32_t mask = (uint32_t)1 << (irq % sizeof(uint32_t));

  if(self->contexts[context].enable[word] & mask)
    self->contexts[context].pendingClaims[word] &= ~mask;

  _processInterrupts(self);
}

static void _cbRead(void *opaque, cpu_addr_t addr, void* buf, size_t size) {
  Plic *self = opaque;

  int claimContext = -1;

  uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    reg[i] = 0;

    if(addr >= sizeof(self->priority[0]) && addr < (sizeof(self->priority) + sizeof(self->priority[0]))) {
      reg[i] = ((uint8_t *)self->priority)[addr - sizeof(self->priority[0])];
    } else if((addr >= 0x1000) && addr < (0x1000 + sizeof(self->pending))) {
      reg[i] = ((uint8_t *)self->pending)[addr - 0x1000];
    } else if((addr >= 0x2000) && addr < 0x200000) {
      const uint32_t a = (uint32_t)addr - 0x2000;
      const unsigned int context = a / 0x80;
      const unsigned int ca = a % 0x80;
      if(context < PLIC_CONTEXTS) {
        if(ca < (sizeof(self->contexts[context].enable))) {
          reg[i] = ((uint8_t *)self->contexts[context].enable)[ca];
        }
      }
    } else if((addr >= 0x200000) && addr < 0x400000) {
      const uint32_t a = (uint32_t)addr - 0x200000;
      const unsigned int context = a / 0x1000;
      const unsigned int ca = a % 0x1000;
      if(context < PLIC_CONTEXTS) {
        if(ca < sizeof(self->contexts[context].threshold)) {
          reg[i] = ((uint8_t *)&self->contexts[context].threshold)[ca];
        } else if(ca >= 4 && ca < (4 + sizeof(self->contexts[context].claim))) {
          reg[i] = ((uint8_t *)&self->contexts[context].claim)[ca - 4];
          claimContext = (int)context;
        }
      }
    }

    ++addr;
  }

  _claimInterrupts(self, claimContext);
}

static void _cbWrite(void *opaque, cpu_addr_t addr, const void* buf, size_t size) {
  Plic *self = opaque;

  int completeContext = -1;
  uint32_t completeIrq = 0;

  const uint8_t *reg = buf;
  for(size_t i = 0; i < size; ++i) {
    if(addr >= sizeof(self->priority[0]) && addr < (sizeof(self->priority) + sizeof(self->priority[0]))) {
      ((uint8_t *)self->priority)[addr - sizeof(self->priority[0])] = reg[i];
    } else if((addr >= 0x2000) && addr < 0x200000) {
      const uint32_t a = (uint32_t)addr - 0x2000;
      const unsigned int context = a / 0x80;
      const unsigned int ca = a % 0x80;
      if(context < PLIC_CONTEXTS) {
        if(ca < (sizeof(self->contexts[context].enable))) {
          ((uint8_t *)self->contexts[context].enable)[ca] = reg[i];
        }
      }
    } else if((addr >= 0x200000) && addr < 0x400000) {
      const uint32_t a = (uint32_t)addr - 0x200000;
      const unsigned int context = a / 0x1000;
      const unsigned int ca = a % 0x1000;
      if(context < PLIC_CONTEXTS) {
        if(ca < sizeof(self->contexts[context].threshold)) {
          ((uint8_t *)&self->contexts[context].threshold)[ca] = reg[i];
        } else if(ca >= 4 && ca < (4 + sizeof(self->contexts[context].claim))) {
          ((uint8_t *)&completeIrq)[ca - 4] = reg[i];
          completeContext = (int)context;
        }
      }
    }

    ++addr;
  }

  _completeInterrupts(self, completeContext, completeIrq);
}

bool plic_init(Plic *self, RV64_Cpu* harts[PLIC_HARTS]) {
  memset(self, 0, sizeof(*self));

  for(int i = 0; i < PLIC_HARTS; ++i)
    self->harts[i] = harts[i];

  self->dev.opaque = self;
  self->dev.size = 0x400000UL;
  self->dev.cbRead = &_cbRead;
  self->dev.cbWrite = &_cbWrite;

  return true;
}

void plic_destroy(Plic *self) {
  (void)self;
}

void plic_interrupt(Plic *self, unsigned int n) {
  if(n == 0 || n > PLIC_INTERRUPTS)
    return;

  const unsigned int word = n / sizeof(uint32_t);

  self->pending[word] |= (uint32_t)1 << (n % sizeof(uint32_t));

  _processInterrupts(self);
}
