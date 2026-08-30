//
// Created by palulukan on 8/30/26.
//

#ifndef RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED
#define RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED

#include "rv64.h"

#include "../log.h"

#include <assert.h>

static inline cpu_word_t _readCSR(RV64_Cpu *self, uint16_t csr) {
  // Reading CSRs can have side effects
  if(self->trap)
    return 0;

  switch(csr) {
    // MSTATUS
    case 0x300: return self->mstatus;

    case 0x301: {
      // MISA
      return CPU_SIGN_BIT // Word size
        | (cpu_word_t)1 << 8    // I
        | (cpu_word_t)1 << 2    // C
        | (cpu_word_t)1 << 0    // A
        | (cpu_word_t)1 << 11;  // M
    }

    // MIE
    case 0x304: return self->mie;

    // MTVEC
    case 0x305: return self->mtvec;

    // MENVCFG
    case 0x30A: return self->menvcfg;

    // MSCRATCH
    case 0x340: return self->mscratch;

    // MEPC
    case 0x341: return self->mepc;

    // MCAUSE
    case 0x342: return self->mcause;

    // MTVAL
    case 0x343: return self->mtval;

    // MIP
    case 0x344: return self->mip;

    // MVENDORID
    case 0xF11:
    // MARCHID
    case 0xF12:
    // MAIMPID
    case 0xF13:
    // MHARTID
    case 0xF14:
      return 0;

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }

  return 0;
}

static inline void _writeCSR(RV64_Cpu *self, uint16_t csr, cpu_word_t val) {
  if(self->trap)
    return;

  switch(csr) {
    case 0x300: {
      // MSTATUS
      self->irq = (self->mstatus & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK)) != (val & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK));
      self->mstatus = MSTATUS_WR_VAL(val);
    }

    // MISA
    case 0x301:
    // MVENDORID
    case 0xF11:
    // MARCHID
    case 0xF12:
    // MAIMPID
    case 0xF13:
    // MHARTID
    case 0xF14:
      break;

    case 0x304: {
      // MIE
      const cpu_word_t newMie = (self->mie & ~MIE_RW_MASK) | (val & MIE_RW_MASK);
      self->irq = self->mie != newMie;
      self->mie = newMie;
      break;
    }

    case 0x305: {
      // MTVEC
      self->mtvec = val;
      break;
    }

    case 0x30A: {
      // MENVCFG
      self->menvcfg = val;
      break;
    }

    case 0x340: {
      // MSCRATCH
      self->mscratch = val;
      break;
    }

    case 0x341: {
      // MEPC
      self->mepc = val & ~(cpu_word_t)1;
      break;
    }

    case 0x343: {
      // MTVAL
      self->mtval = val;
      break;
    }

    case 0x344: {
      // MIP
      const cpu_word_t newMip = (self->mip & ~MIP_RW_MASK) | (val & MIP_RW_MASK);
      self->irq = self->mip != newMip;
      self->mip = newMip;
      break;
    }

    default: {
      WARN("Writing unknown CSR: %x", csr);
      _trap(self, MCAUSE_INST_ILL, false);
      break;
    }
  }
}

#endif //RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED
