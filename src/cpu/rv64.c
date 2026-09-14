//
// Created by palulukan on 7/24/26.
//

#include "rv64.h"

#include "../log.h"

#include "rv_internal.h"
#include "rv_instr.h"
#include "rv_cext.h"

#include <string.h>
#include <assert.h>

static inline void _processPendingInterrupts(RV64_Cpu *self) {
  static const unsigned int vectors[] = {
    MCAUSE_MACHINE_EXT_INT,
    MCAUSE_MACHINE_SW_INT,
    MCAUSE_MACHINE_TMR_INT,
    // MCAUSE_SUPERVISOR_EXT_INT,
    // MCAUSE_SUPERVISOR_SW_INT,
    // MCAUSE_SUPERVISOR_TMR_INT,
    // MCAUSE_CTR_OVF_INT,
  };

  if(!(self->mstatus & MSTATUS_MIE_MASK))
    return;

  const cpu_word_t interrupts = self->mip & self->mie;
  if(interrupts) {
    for(int i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
      const cpu_word_t mask = 1ULL << vectors[i];
      if(interrupts & mask) {
        _trap(self, vectors[i], true);
        break;
      }
    }
  }
}

static inline uint32_t _fetch(RV64_Cpu *self) {
  const cpu_word_t pc = _readPC(self);

  if(pc & 1) {
    _trap(self, MCAUSE_INST_ALLIGN, false);
    return 0;
  }

  uint32_t res = 0;

  cpu_addr_t offset = pc - self->icache.base;
  if(self->icache.base == CPU_ADDR_MAX || offset > (ICACHE_LINE_SIZE - sizeof(res))) {
    if(!bus_read(self->bus, pc, self->icache.line, ICACHE_LINE_SIZE)) {
      // Very end of the memory, less than a cache line size
      if(!bus_read(self->bus, pc, &res, sizeof(res))) {
        // Out of a valid address space
        _trap(self, MCAUSE_INST_AF, false);
        return 0;
      }

      return res;
    }
    self->icache.base = pc;

    // Cache line loaded, we are at the start of the cache line
    offset = 0;

#ifdef CPU_STATS
    ++self->icacheMisses;
#endif
  } else {
#ifdef CPU_STATS
    ++self->icacheHits;
#endif
  }

  memcpy(&res, self->icache.line + offset, sizeof(res));
  return res;
}

static inline void _doTrap(RV64_Cpu* self) {
  self->trap = false;
  self->wfi = false;

  cpu_word_t vec = self->mtvec & ~(cpu_word_t)3;
  if(self->mcause & CPU_SIGN_BIT) {
    // Interrupt
    const cpu_word_t mode = self->mtvec & 3;
    if(mode == 1)
      vec += 4 * (self->mcause & ~CPU_SIGN_BIT);
  } else {
    // DEBUG("RV64: Executing trap @%" PRI_CPU_PTR " -> %" PRI_CPU_PTR " cause: %" PRI_CPU_XWORD, _readPC(self), vec, self->mcause);
  }

  // Copy MIE -> MPIE and clear MIE
  self->mstatus = (self->mstatus & ~(MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK)) | ((self->mstatus & MSTATUS_MIE_MASK) ? MSTATUS_MPIE_MASK : 0);

  // Save privilege mode
  self->mstatus = (self->mstatus & ~MSTATUS_MPP_MASK) | (cpu_word_t)self->mode << 11;

  self->mode = RV64_PRIV_MODE_MACHINE;

  self->mepc = self->PC;

  _writePC(self, vec);
}

bool rv64_init(RV64_Cpu* self, Bus *bus) {
  memset(self, 0, sizeof(*self));

  self->bus = bus;

  return true;
}

void rv64_destroy(RV64_Cpu *self) {
  (void)self;

#ifdef CPU_STATS
  DEBUG("CPU stats:\n\ticacheHits:\t%" PRIu32 "\n\ticacheMisses:\t%" PRIu32,
    self->icacheHits, self->icacheMisses);
#endif
}

void rv64_reset(RV64_Cpu *self, cpu_addr_t start) {
  _flushIcache(self);

  bus_reservationCheckInvalidate(self->bus, 0, 0);

  self->wfi = false;

  self->mstatus = MSTATUS_WR_VAL(0U);

  self->irq = false;

  self->trap = false;

  self->mode = RV64_PRIV_MODE_MACHINE;

  _writePC(self, start);
}

void rv64_run(RV64_Cpu *self) {
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

void rv64_setInterrupt(RV64_Cpu *self, RV64_MCAUSE n) {
  const cpu_word_t mask = (cpu_word_t)1 << n;

  self->mip |= mask;

  self->irq = true;
  self->wfi = false;
}

void rv64_clearInterrupt(RV64_Cpu *self, RV64_MCAUSE n) {
  self->mip &= ~((cpu_word_t)1 << n);
}
