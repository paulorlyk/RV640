//
// Created by palulukan on 8/30/26.
//

#ifndef RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858
#define RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858

#include "rv.h"

static inline void _flushIcache(RV_Cpu *self) {
  self->icache.base = CPU_ADDR_MAX;
}

static inline void _interrupt(RV_Cpu *self, RV_IrqCause cause, bool delegate) {
  self->trap = true;
  self->trapCause = ((cpu_word_t)cause) | CPU_SIGN_BIT;
  self->trapDelegate = delegate;

  rv_clearInterrupt(self, cause);
}

static inline void _trap(RV_Cpu *self, RV_TrapCause cause) {
  self->trap = true;
  self->trapCause = cause;
  self->trapDelegate = (cause != TRAP_CALL_M)
                      && (cause != TRAP_DOUBLE_TRAP)
                      && (self->mode <= RV_PRIV_MODE_SUPERVISOR)
                      && (self->csr.medeleg & (cpu_word_t)1 << cause);
}

static inline cpu_word_t _readReg(const RV_Cpu *self, unsigned int rd) {
  return self->regs.Rx[rd];
}

static inline void _writeReg(RV_Cpu *self, unsigned int rd, cpu_word_t d) {
  if(!rd || self->trap)
    return;

  self->regs.Rx[rd] = d;
}

static inline cpu_addr_t _readPC(const RV_Cpu *self) {
  return self->PC;
}

static inline void _writePC(RV_Cpu *self, cpu_addr_t d) {
  if(self->trap)
    return;

  self->PC = d;
}

static inline void _readMem(RV_Cpu *self, cpu_addr_t addr, void* buf, size_t size) {
  // Reading memory can have side effects
  if(self->trap)
    return;

  if(!bus_read(self->bus, addr, buf, size))
    _trap(self, TRAP_LD_AF);
}

static inline void _writeMem(RV_Cpu *self, cpu_addr_t addr, const void* buf, size_t size) {
  if(self->trap)
    return;

  if(!bus_write(self->bus, addr, buf, size))
    _trap(self, TRAP_ST_AF);
}

static inline void _pendingIRQ(RV_Cpu *self) {
  self->irq = true;
  self->wfi = false;
}

static inline void _writeMstatus(RV_Cpu *self, cpu_word_t val) {
  if((self->csr.mstatus & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK)) != (val & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK)))
    _pendingIRQ(self);

  RV_PrivMode mpp = MSTATUS_GET_MPP(val);
  if(mpp != RV_PRIV_MODE_USER && mpp != RV_PRIV_MODE_SUPERVISOR)
    mpp = RV_PRIV_MODE_MACHINE;

  self->csr.mstatus = (MSTATUS_WR_VAL(val) & ~MSTATUS_MPP_MASK) | MSTATUS_MPP(mpp);
}

static inline void _writeMie(RV_Cpu *self, cpu_word_t val) {
  const cpu_word_t newMie = (self->csr.mie & ~MIE_RW_MASK) | (val & MIE_RW_MASK);
  if(self->csr.mie != newMie)
    _pendingIRQ(self);

  self->csr.mie = newMie;
}

static inline void _writeMip(RV_Cpu *self, cpu_word_t val) {
  const cpu_word_t newMip = (self->csr.mip & ~MIP_RW_MASK) | (val & MIP_RW_MASK);
  if(self->csr.mip != newMip)
    _pendingIRQ(self);

  self->csr.mip = newMip;
}

static inline void _xret(RV_Cpu *self, RV_PrivMode mode, int instSize) {
  if(self->mode < mode) {
    _trap(self, TRAP_INST_ILL);
    return;
  }

  if(self->mode == RV_PRIV_MODE_SUPERVISOR && mode == RV_PRIV_MODE_SUPERVISOR && (self->csr.mstatus & MSTATUS_TSR_MASK)) {
    _trap(self, TRAP_INST_ILL);
    return;
  }

  if(mode == RV_PRIV_MODE_SUPERVISOR) {
    // Restore SIE
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_SIE_MASK) | ((self->csr.mstatus & MSTATUS_SPIE_MASK) ? MSTATUS_SIE_MASK : 0) | MSTATUS_SPIE_MASK;
  } else {
    // Restore MIE
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_MIE_MASK) | ((self->csr.mstatus & MSTATUS_MPIE_MASK) ? MSTATUS_MIE_MASK : 0) | MSTATUS_MPIE_MASK;
  }

  // Restore privilege mode
  if(mode == RV_PRIV_MODE_SUPERVISOR) {
    self->mode = MSTATUS_GET_SPP(self->csr.mstatus);
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_SPP_MASK) | MSTATUS_SPP(RV_PRIV_MODE_USER);
  } else {
    self->mode = MSTATUS_GET_MPP(self->csr.mstatus);
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_MPP_MASK) | MSTATUS_MPP(RV_PRIV_MODE_USER);
  }

  if(self->mode <= RV_PRIV_MODE_SUPERVISOR)
    self->csr.mstatus &= ~MSTATUS_MPRV_MASK;

  _writePC(self, (mode == RV_PRIV_MODE_SUPERVISOR ? self->csr.sepc : self->csr.mepc) - instSize);
}

#endif //RV_INTERNAL_H_4B139402F2B845E4ACBE829B553A8858
