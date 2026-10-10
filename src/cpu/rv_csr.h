//
// Created by palulukan on 8/30/26.
//

#ifndef RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED
#define RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED

#include "rv.h"

#include "rv_internal.h"

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

static inline bool _checkPrfCounterAccess(RV_Cpu *self, cpu_word_t mask) {
  if(self->mode < RV_PRIV_MODE_MACHINE && !(self->csr.mcounteren & mask))
    return false;

  if(self->mode < RV_PRIV_MODE_SUPERVISOR && !(self->csr.scounteren & mask))
    return false;

  return true;
}

static inline cpu_word_t _readCSR(RV_Cpu *self, uint16_t csr) {
  // Reading CSRs can have side effects
  if(self->trap)
    return 0;

  const RV_PrivMode csrPrivMode = (RV_PrivMode)((unsigned int)(csr >> 8) & 3);
  if(self->mode < csrPrivMode) {
    _trap(self, TRAP_INST_ILL);
    return 0;
  }

  switch(csr) {
    // SSTATUS
    case 0x100: return self->csr.mstatus & ~SSTATUS_WPRI_MASK;

    // SIE
    case 0x104: return self->csr.mie & self->csr.mideleg & SIE_RW_MASK;

    // STVEC
    case 0x105: return self->csr.stvec;

    // SCOUNTEREN
    case 0x106: return self->csr.scounteren;

    // SENVCFG
    case 0x10A: return self->csr.senvcfg;

    // MSCRATCH
    case 0x140: return self->csr.sscratch;

    // SEPC
    case 0x141: return self->csr.sepc;

    // SCAUSE
    case 0x142: return self->csr.scause;

    // STVAL
    case 0x143: return self->csr.stval;

    // SIP
    case 0x144: return self->csr.mip & self->csr.mideleg & SIP_RW_MASK;

    // SATP
    case 0x180: return 0;

    // MSTATUS
    case 0x300: return self->csr.mstatus;

    case 0x301: {
      // MISA
#ifdef CONFIG_RV64
      const cpu_word_t mxl = CPU_SIGN_BIT;
#else
      const cpu_word_t mxl = CPU_SIGN_BIT >> 1;
#endif
      return mxl
        | MISA_EXT_BIT('a')
        | MISA_EXT_BIT('c')
        | MISA_EXT_BIT('i')
        | MISA_EXT_BIT('m')
        | MISA_EXT_BIT('s')
        | MISA_EXT_BIT('u');
    }

    // MEDELEG
    case 0x302: return self->csr.medeleg;

    // MIDELEG
    case 0x303: return self->csr.mideleg;

    // MIE
    case 0x304: return self->csr.mie;

    // MTVEC
    case 0x305: return self->csr.mtvec;

    // MCOUNTEREN
    case 0x306: return self->csr.mcounteren;

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

    // CYCLE
    case 0xC00: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_CY_MASK))
        break;

      return 0;
    }

    // TIME
    case 0xC01: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_TM_MASK))
        break;

      return aclint_getMtime(self->aclint);
    }

    // INSTRET
    case 0xC02: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_IR_MASK))
        break;

      return 0;
    }

#ifndef CONFIG_RV64
    // CYCLEH
    case 0xC80: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_CY_MASK))
        break;

      return 0;
    }

    // TIMEH
    case 0xC81: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_TM_MASK))
        break;

      return aclint_getMtime(self->aclint) >> 32;
    }

    // INSTRETH
    case 0xC82: {
      if(!_checkPrfCounterAccess(self, COUNTEREN_IR_MASK))
        break;

      return 0;
    }
#endif

    // MVENDORID
    case 0xF11: return RV_MVENDORID;

    // MARCHID
    case 0xF12: return RV_MARCHID;

    // MIMPID
    case 0xF13: return RV_MIMPID;

    // MHARTID
    case 0xF14: return self->csr.mhartid;

    default:
      break;
  }

  _trap(self, TRAP_INST_ILL);
  return 0;
}

static inline void _writeCSR(RV_Cpu *self, uint16_t csr, cpu_word_t val) {
  if(self->trap)
    return;

  const RV_PrivMode csrPrivMode = (RV_PrivMode)((unsigned int)(csr >> 8) & 3);
  if(self->mode < csrPrivMode) {
    _trap(self, TRAP_INST_ILL);
    return;
  }

  switch(csr) {
    case 0x100: {
      // SSTATUS
      _writeMstatus(self, (val & ~SSTATUS_WPRI_MASK) | (self->csr.mstatus & SSTATUS_WPRI_MASK));
      break;
    }

    case 0x104: {
      // SIE
      const cpu_word_t mask = self->csr.mideleg & SIE_RW_MASK;
      _writeMie(self, (val & mask) | (self->csr.mie & ~mask));
      break;
    }

    case 0x105: {
      // STVEC
      self->csr.stvec = val;
      break;
    }

    case 0x106: {
      // SCOUNTEREN
      self->csr.scounteren = val & COUNTEREN_WR_MASK;
      break;
    }

    case 0x10A: {
      // SENVCFG
      self->csr.senvcfg = val;
      break;
    }

    case 0x140: {
      // SSCRATCH
      self->csr.sscratch = val;
      break;
    }

    case 0x141: {
      // SEPC
      self->csr.sepc = val & ~(cpu_word_t)1;
      break;
    }

    case 0x143: {
      // STVAL
      self->csr.stval = val;
      break;
    }

    case 0x144: {
      // SIP
      const cpu_word_t mask = self->csr.mideleg & SIP_RW_MASK;
      _writeMip(self, (val & mask) | (self->csr.mip & ~mask));
      break;
    }

    case 0x300: {
      // MSTATUS
      _writeMstatus(self, val);
      break;
    }

    // MISA
    case 0x301:
      break;

    // MEDELEG
    case 0x302: {
      self->csr.medeleg = val & MEDELEG_RW_MASK;
      break;
    }

    // MIDELEG
    case 0x303: {
      self->csr.mideleg = val & MIDELEG_RW_MASK;
      break;
    }

    case 0x304: {
      // MIE
      _writeMie(self, val);
      break;
    }

    case 0x305: {
      // MTVEC
      self->csr.mtvec = val;
      break;
    }

    case 0x306: {
      // MCOUNTEREN
      self->csr.mcounteren = val & COUNTEREN_WR_MASK;
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
      _writeMip(self, val);
      break;
    }

    default: {
      _trap(self, TRAP_INST_ILL);
      break;
    }
  }
}

#endif //RV_CSR_H_D4C96AF09E5C4CCA803F631D77C3F3ED
