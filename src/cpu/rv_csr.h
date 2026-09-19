//
// Created by palulukan on 8/30/26.
//

#ifndef RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED
#define RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED

#include "rv.h"

#include "../log.h"

#include <assert.h>

static inline cpu_word_t _readCSR(RV_Cpu *self, uint16_t csr) {
  // Reading CSRs can have side effects
  if(self->trap)
    return 0;

  switch(csr) {
    // MSTATUS
    case 0x300: return self->csr.mstatus;

    case 0x301: {
      // MISA
      return CPU_SIGN_BIT // Word size
        | (cpu_word_t)1 << 8    // I
        | (cpu_word_t)1 << 2    // C
        | (cpu_word_t)1 << 0    // A
        | (cpu_word_t)1 << 11;  // M
    }

    // MIE
    case 0x304: return self->csr.mie;

    // MTVEC
    case 0x305: return self->csr.mtvec;

    // MENVCFG
    case 0x30A: return self->csr.menvcfg;

#ifndef CONFIG_RV64
    // MSTATUSH
    case 0x310: return self->csr.mstatush;
#endif

    // MSCRATCH
    case 0x340: return self->csr.mscratch;

    // MEPC
    case 0x341: return self->csr.mepc;

    // MCAUSE
    case 0x342: return self->csr.mcause;

    // MTVAL
    case 0x343: return self->csr.mtval;

    // MIP
    case 0x344: return self->csr.mip;

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

static inline void _writeCSR(RV_Cpu *self, uint16_t csr, cpu_word_t val) {
  if(self->trap)
    return;

  switch(csr) {
    case 0x300: {
      // MSTATUS
      self->irq = (self->csr.mstatus & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK)) != (val & (MSTATUS_MIE_MASK | MSTATUS_SIE_MASK));
      self->csr.mstatus = MSTATUS_WR_VAL(val);
      break;
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
      const cpu_word_t newMie = (self->csr.mie & ~MIE_RW_MASK) | (val & MIE_RW_MASK);
      self->irq = self->csr.mie != newMie;
      self->csr.mie = newMie;
      break;
    }

    case 0x305: {
      // MTVEC
      self->csr.mtvec = val;
      break;
    }

    case 0x30A: {
      // MENVCFG
      self->csr.menvcfg = val;
      break;
    }

#ifndef CONFIG_RV64
    case 0x310: {
      // MSTATUSH
      self->csr.mstatush = MSTATUSH_WR_VAL(val);
      break;
    }
#endif

    case 0x340: {
      // MSCRATCH
      self->csr.mscratch = val;
      break;
    }

    case 0x341: {
      // MEPC
      self->csr.mepc = val & ~(cpu_word_t)1;
      break;
    }

    case 0x343: {
      // MTVAL
      self->csr.mtval = val;
      break;
    }

    case 0x344: {
      // MIP
      const cpu_word_t newMip = (self->csr.mip & ~MIP_RW_MASK) | (val & MIP_RW_MASK);
      self->irq = self->csr.mip != newMip;
      self->csr.mip = newMip;
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
