//
// Created by palulukan on 7/24/26.
//

#include "rv.h"

#include "../log.h"

#include "rv_internal.h"
#include "rv_base.h"
#include "rv_cext.h"

#include "../sbi.h"

#include <string.h>

static inline void _processPendingMInterrupts(RV_Cpu *self) {
  static const unsigned int vectors[] = {
    IRQ_MACHINE_EXT_INT,
    IRQ_MACHINE_SW_INT,
    IRQ_MACHINE_TMR_INT,
  };
  static const cpu_word_t masks[] = {
    (cpu_word_t)1 << (unsigned int)IRQ_MACHINE_EXT_INT,
    (cpu_word_t)1 << (unsigned int)IRQ_MACHINE_SW_INT,
    (cpu_word_t)1 << (unsigned int)IRQ_MACHINE_TMR_INT,
  };

  const cpu_word_t interrupts = self->csr.mip & self->csr.mie;
  if(interrupts) {
    for(int i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
      if(interrupts & masks[i]) {
        _interrupt(self, vectors[i], false);
        break;
      }
    }
  }
}

static inline void _processPendingSInterrupts(RV_Cpu *self) {
  static const unsigned int vectors[] = {
    IRQ_SUPERVISOR_EXT_INT,
    IRQ_SUPERVISOR_SW_INT,
    IRQ_SUPERVISOR_TMR_INT,
    IRQ_CTR_OVF_INT,
  };
  static const cpu_word_t masks[] = {
    (cpu_word_t)1 << (unsigned int)IRQ_SUPERVISOR_EXT_INT,
    (cpu_word_t)1 << (unsigned int)IRQ_SUPERVISOR_SW_INT,
    (cpu_word_t)1 << (unsigned int)IRQ_SUPERVISOR_TMR_INT,
    (cpu_word_t)1 << (unsigned int)IRQ_CTR_OVF_INT,
  };

  const cpu_word_t interrupts = self->csr.mip & self->csr.mie;
  if(interrupts) {
    const bool sie = self->csr.mstatus & MSTATUS_SIE_MASK;

    for(int i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
      if(masks[i] & interrupts) {
        const bool delegated = self->csr.mideleg & masks[i];

        const bool irq = !delegated || ((self->mode < RV_PRIV_MODE_SUPERVISOR) || (self->mode == RV_PRIV_MODE_SUPERVISOR && sie) || (self->mode == RV_PRIV_MODE_MACHINE && !sie));
        if(irq) {
          _interrupt(self, vectors[i], delegated);
          break;
        }
      }
    }
  }
}

static inline void _processPendingInterrupts(RV_Cpu *self) {
  if(self->mode == RV_PRIV_MODE_MACHINE && (self->csr.mstatus & MSTATUS_MIE_MASK) == 0)
    return;

  if(!self->trap)
    _processPendingMInterrupts(self);

  if(!self->trap)
    _processPendingSInterrupts(self);
}

static inline uint32_t _fetch(RV_Cpu *self) {
  const cpu_addr_t pc = _readPC(self);

  if(pc & 1) {
    _trap(self, TRAP_INST_ALLIGN);
    return 0;
  }

  uint32_t res = 0;

  cpu_addr_t offset = pc - self->icache.base;
  if(self->icache.base == CPU_ADDR_MAX || offset > (ICACHE_LINE_SIZE - sizeof(res))) {
#ifdef CPU_STATS
    ++self->icacheMisses;
#endif

    if(!bus_read(self->bus, pc, self->icache.line, ICACHE_LINE_SIZE)) {
      // Very end of the memory, less than a cache line size
      if(!bus_read(self->bus, pc, &res, sizeof(res))) {
        // Out of a valid address space
        _trap(self, TRAP_INST_AF);
        return 0;
      }

      return res;
    }
    self->icache.base = pc;

    // Cache line loaded, we are at the start of the cache line
    offset = 0;
  } else {
#ifdef CPU_STATS
    ++self->icacheHits;
#endif
  }

  memcpy(&res, self->icache.line + offset, sizeof(res));
  return res;
}

static inline void _doTrap(RV_Cpu* self) {
  self->trap = false;
  self->wfi = false;

  const bool irq = self->trapCause & CPU_SIGN_BIT;
  const cpu_word_t cause = (self->trapCause & ~CPU_SIGN_BIT);

  cpu_word_t xtvec;
  if(!self->trapDelegate) {
    xtvec = self->csr.mtvec;

    self->csr.mcause = self->trapCause;

    // Copy MIE -> MPIE and clear MIE
    self->csr.mstatus = (self->csr.mstatus & ~(MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK)) | ((self->csr.mstatus & MSTATUS_MIE_MASK) ? MSTATUS_MPIE_MASK : 0);

    // Save privilege mode
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_MPP_MASK) | MSTATUS_MPP(self->mode);

    self->mode = RV_PRIV_MODE_MACHINE;

    self->csr.mtval = 0;
    self->csr.mepc = _readPC(self);
  } else {
    xtvec = self->csr.stvec;

    self->csr.scause = self->trapCause;

    // Copy SIE -> SPIE and clear SIE
    self->csr.mstatus = (self->csr.mstatus & ~(MSTATUS_SPIE_MASK | MSTATUS_SIE_MASK)) | ((self->csr.mstatus & MSTATUS_SIE_MASK) ? MSTATUS_SPIE_MASK : 0);

    // Save privilege mode
    self->csr.mstatus = (self->csr.mstatus & ~MSTATUS_SPP_MASK) | MSTATUS_SPP(self->mode);

    self->mode = RV_PRIV_MODE_SUPERVISOR;

    self->csr.stval = 0;
    self->csr.sepc = _readPC(self);
  }

  cpu_word_t vec = xtvec & ~(cpu_word_t)3;
  if(irq && (xtvec & 3) == 1)
    vec += 4 * cause;

  if(self->mode == RV_PRIV_MODE_MACHINE && self->virtualSBI) {
    if(cause == TRAP_CALL_S) {
      sbi_handleEcall(self);
      _xret(self, RV_PRIV_MODE_MACHINE, -4);
    } else if(irq) {
      sbi_handleInterrupt(self);
      _xret(self, RV_PRIV_MODE_MACHINE, 0);
    } else {
      sbi_handleException(self);
      _xret(self, RV_PRIV_MODE_MACHINE, 0);
    }
  } else {
    _writePC(self, vec);
  }
}

bool rv_init(RV_Cpu* self, Bus *bus, Aclint *aclint, int hartId, bool virtualSBI) {
  memset(self, 0, sizeof(*self));

  self->bus = bus;
  self->aclint = aclint;

  self->virtualSBI = virtualSBI;

  self->csr.mhartid = hartId;

  return true;
}

void rv_destroy(RV_Cpu *self) {
  (void)self;

#ifdef CPU_STATS
  DEBUG("CPU stats:\n\ticacheHits:\t%" PRIu32 "\n\ticacheMisses:\t%" PRIu32,
    self->icacheHits, self->icacheMisses);
#endif
}

void rv_reset(RV_Cpu *self, RV_PrivMode mode, cpu_addr_t start) {
  _flushIcache(self);

  bus_reservationCheckInvalidate(self->bus, 0, 0);

  self->wfi = false;

  self->csr.mstatus = MSTATUS_WR_VAL(0U);
#ifndef CONFIG_RV64
  self->csr.mstatush = MSTATUSH_WR_VAL(0U);
#endif

  self->csr.menvcfg = MENVCFG_CBZE_MASK;
  self->csr.senvcfg = SENVCFG_CBZE_MASK;

  self->irq = false;

  self->trap = false;

  self->mode = mode;

  _writePC(self, start);
}

void rv_run(RV_Cpu *self) {
  if(self->trap) {
    _doTrap(self);
  } else if(self->irq) {
    _processPendingInterrupts(self);
    if(self->trap)
      return;
  }

  const uint32_t instr = _fetch(self);
  if(self->trap)
    return;

  int instrSize = 2;
  if(!_execCext(self, instr)) {
    instrSize = 4;
    _execInstr(self, instr);
  }

  _writePC(self, _readPC(self) + instrSize);
}

void rv_setInterrupt(RV_Cpu *self, RV_IrqCause n) {
  const cpu_word_t mask = (cpu_word_t)1 << n;

  self->csr.mip |= mask;

  _pendingIRQ(self);
}

void rv_clearInterrupt(RV_Cpu *self, RV_IrqCause n) {
  const cpu_word_t mask = (cpu_word_t)1 << n;

  self->csr.mip &= ~mask;
}
