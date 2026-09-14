//
// Created by palulukan on 8/30/26.
//

#ifndef RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858
#define RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858

#include "rv64.h"

static inline void _flushIcache(RV64_Cpu *self) {
  self->icache.base = CPU_ADDR_MAX;
}

static inline void _trap(RV64_Cpu *self, RV64_MCAUSE cause, bool interrupt) {
  self->trap = true;

  self->mcause = cause;
  if(interrupt)
    self->mcause |= CPU_SIGN_BIT;
}

static inline cpu_word_t _readReg(const RV64_Cpu *self, unsigned int rd) {
  return self->regs.Rx[rd];
}

static inline void _writeReg(RV64_Cpu *self, unsigned int rd, cpu_word_t d) {
  if(!rd || self->trap)
    return;

  self->regs.Rx[rd] = d;
}

static inline cpu_addr_t _readPC(const RV64_Cpu *self) {
  return self->PC;
}

static inline void _writePC(RV64_Cpu *self, cpu_addr_t d) {
  if(self->trap)
    return;

  self->PC = d;
}

static inline void _readMem(RV64_Cpu *self, cpu_addr_t addr, void* buf, size_t size) {
  // Reading memory can have side effects
  if(self->trap)
    return;

  if(!bus_read(self->bus, addr, buf, size))
    _trap(self, MCAUSE_LD_AF, false);
}

static inline void _writeMem(RV64_Cpu *self, cpu_addr_t addr, const void* buf, size_t size) {
  if(self->trap)
    return;

  if(!bus_write(self->bus, addr, buf, size))
    _trap(self, MCAUSE_ST_AF, false);
}

#endif //RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858
