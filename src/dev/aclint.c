//
// Created by palulukan on 8/9/26.
//

#include "aclint.h"

#include <string.h>

static void _updateMtimer(Aclint *self) {
  for(int i = 0; i < ACLINT_HARTS; ++i) {
    if(self->mtimer.mtime >= self->mtimer.mtimecmp[i]) {
      if(!self->mtimer.pindingIrq[i]) {
        self->mtimer.pindingIrq[i] = true;
        rv64_setInterrupt(self->harts[i], MCAUSE_MACHINE_TMR_INT);
      }
    } else {
      if(self->mtimer.pindingIrq[i]) {
        self->mtimer.pindingIrq[i] = false;
        rv64_clearInterrupt(self->harts[i], MCAUSE_MACHINE_TMR_INT);
      }
    }
  }
}

static void _updateMswi(const Aclint *self) {
  for(int i = 0; i < ACLINT_HARTS; ++i) {
    if(self->mswi.msip[i])
      rv64_setInterrupt(self->harts[i], MCAUSE_MACHINE_SW_INT);
    else
      rv64_clearInterrupt(self->harts[i], MCAUSE_MACHINE_SW_INT);
  }
}

static void _cbRead(void *opaque, cpu_addr_t addr, void* buf, size_t size) {
  Aclint *self = opaque;

  uint8_t *b = buf;
  for(size_t i = 0; i < size; ++i) {
    b[i] = 0;

    if(addr < sizeof(self->mswi.msip))
      b[i] = ((uint8_t *)&self->mswi.msip)[addr];
    else if(addr >= 0x4000 && addr < (0x4000 + sizeof(self->mtimer.mtimecmp)))
      b[i] = ((uint8_t *)&self->mtimer.mtimecmp)[addr - 0x4000];
    else if(addr >= 0xBFF8 && addr < (0xBFF8 + sizeof(self->mtimer.mtime)))
      b[i] = ((uint8_t *)&self->mtimer.mtime)[addr - 0xBFF8];

    ++addr;
  }
}

static void _cbWrite(void *opaque, cpu_addr_t addr, const void* buf, size_t size) {
  Aclint *self = opaque;

  const uint8_t *b = (uint8_t *)buf;
  for(size_t i = 0; i < size; ++i) {
    if(addr < sizeof(self->mswi.msip))
      ((uint8_t *)&self->mswi.msip)[addr] = b[i];
    else if(addr >= 0x4000 && addr < (0x4000 + sizeof(self->mtimer.mtimecmp)))
      ((uint8_t *)&self->mtimer.mtimecmp)[addr - 0x4000] = b[i];
    else if(addr >= 0xBFF8 && addr < (0xBFF8 + sizeof(self->mtimer.mtime)))
      ((uint8_t *)&self->mtimer.mtime)[addr - 0xBFF8] = b[i];

    ++addr;
  }

  _updateMtimer(self);
  _updateMswi(self);
}

bool aclint_init(Aclint *self, RV64_Cpu* harts[ACLINT_HARTS]) {
  memset(self, 0, sizeof(*self));

  for(int i = 0; i < ACLINT_HARTS; ++i)
    self->harts[i] = harts[i];

  self->dev.opaque = self;
  self->dev.size = 0x10000UL;
  self->dev.cbRead = &_cbRead;
  self->dev.cbWrite = &_cbWrite;

  return true;
}

void aclint_destroy(Aclint *self) {
  (void)self;
}

void aclint_tick(Aclint *self, unsigned int cycles) {
  self->mtimer.mtime += cycles;

  _updateMtimer(self);
}

uint64_t aclint_mtimeRemains(Aclint *self, int hart) {
  return self->mtimer.mtimecmp[hart] - self->mtimer.mtime;
}
