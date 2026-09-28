//
// Created by palulukan on 9/27/26.
//

#include "sbi.h"

#include "log.h"

#include "dev/aclint.h"

#include "ui.h"

#include <assert.h>

static inline SBI_Ret _handleBaseExt(RV_Cpu *hart, cpu_word_t fid) {
  SBI_Ret res = {0};

  switch(fid) {
    case 0: {
      // Get SBI specification version
      // 2.0
      res.uvalue = (cpu_word_t)2 << 24 | 0;
      break;
    }

    case 1: {
      // Get SBI implementation ID
      res.uvalue = 0;
      break;
    }

    case 2: {
      // Get SBI implementation version
      res.uvalue = 0x1000;
      break;
    }

    case 3: {
      // Probe SBI extension
      const cpu_word_t extId = rv_peekRx(hart, CPU_REG_A0);
      res.uvalue = extId == SBI_EID_BASE || extId == SBI_EID_TIMER || extId == SBI_EID_IPI || extId == SBI_EID_RFENCE || extId == SBI_EID_SYSRESET || extId == SBI_EID_DBGCON || extId == SBI_EID_FWFT;
      break;
    }

    case 4: {
      // Get machine vendor ID
      res.uvalue = RV_MVENDORID;
      break;
    }

    case 5: {
      // Get machine architecture ID
      res.uvalue = RV_MARCHID;
      break;
    }

    case 6: {
      // Get machine implementation ID
      res.uvalue = RV_MIMPID;
      break;
    }

    default: {
      res.error = SBI_ERR_NOT_SUPPORTED;
      break;
    }
  }

  return res;
}

static inline SBI_Ret _handleTimerExt(RV_Cpu *hart, cpu_word_t fid) {
  SBI_Ret res = {0};

  switch(fid) {
    case 0: {
#ifdef CONFIG_RV64
      const uint64_t time = rv_peekRx(hart, CPU_REG_A0);
#else
      const uint64_t time = (uint64_t)rv_peekRx(hart, CPU_REG_A0) | ((uint64_t)rv_peekRx(hart, CPU_REG_A1) << 32);
#endif
      aclint_setMtimecmp(hart->aclint, (int)hart->csr.mhartid, time);
      if(time == ~(uint64_t)1)
        hart->csr.mie &= ~((cpu_word_t)1 << (unsigned int)IRQ_MACHINE_TMR_INT);
      else
        hart->csr.mie |= (cpu_word_t)1 << (unsigned int)IRQ_MACHINE_TMR_INT;
      break;
    }

    default: {
      res.error = SBI_ERR_NOT_SUPPORTED;
      break;
    }
  }

  return res;
}

static inline SBI_Ret _handleDbgConExt(RV_Cpu *hart, cpu_word_t fid) {
  SBI_Ret res = {0};

  switch(fid) {
    case 0: {
      // Console Write
      const cpu_word_t nBytes = rv_peekRx(hart, CPU_REG_A0);
      const cpu_word_t addrL = rv_peekRx(hart, CPU_REG_A1);
      // const cpu_word_t addrH = rv_peekRx(hart, CPU_REG_A2);

      for(cpu_word_t i = 0; i < nBytes; ++i) {
        char b;
        if(!bus_read(hart->bus, addrL + i, &b, sizeof(b))) {
          res.error = SBI_ERR_INVALID_PARAM;
          break;
        }

        ui_putch(b);
      }

      res.uvalue = nBytes;
      break;
    }

    default: {
      res.error = SBI_ERR_NOT_SUPPORTED;
      break;
    }
  }

  return res;
}

void sbi_startHart(RV_Cpu *hart, cpu_addr_t entryPoint, cpu_word_t a1) {
  INFO("SBI: Resetting the CPU...");

  rv_reset(hart, RV_PRIV_MODE_SUPERVISOR, entryPoint);

  rv_pokeRx(hart, CPU_REG_A0, rv_getHartID(hart));
  rv_pokeRx(hart, CPU_REG_A1, a1);

  hart->csr.medeleg = CPU_UINT_MAX & ~(((cpu_word_t)1 << (unsigned int)TRAP_CALL_S) | ((cpu_word_t)1 << (unsigned int)TRAP_CALL_M) | ((cpu_word_t)1 << (unsigned int)TRAP_DOUBLE_TRAP));
  hart->csr.mideleg = CPU_UINT_MAX & ~(((cpu_word_t)1 << (unsigned int)IRQ_MACHINE_SW_INT) | ((cpu_word_t)1 << (unsigned int)IRQ_MACHINE_TMR_INT) | ((cpu_word_t)1 << (unsigned int)IRQ_MACHINE_EXT_INT));
}

void sbi_handleEcall(RV_Cpu *hart) {
  const cpu_word_t eid = rv_peekRx(hart, CPU_REG_A7);
  const cpu_word_t fid = rv_peekRx(hart, CPU_REG_A6);

  SBI_Ret res = {0};

  switch(eid) {
    case SBI_EID_BASE: res = _handleBaseExt(hart, fid); break;
    case SBI_EID_TIMER: res = _handleTimerExt(hart, fid); break;
    case SBI_EID_DBGCON: res = _handleDbgConExt(hart, fid); break;

    default: {
      ERROR("SBI: Unknown extension: 0x%" PRI_CPU_XWORD, eid);
      res.error = SBI_ERR_NOT_SUPPORTED;
      assert(false);
      break;
    }
  }

  rv_pokeRx(hart, CPU_REG_A0, res.error);
  rv_pokeRx(hart, CPU_REG_A1, res.uvalue);
}

void sbi_handleInterrupt(RV_Cpu *hart) {
  cpu_word_t irq = hart->csr.mcause & ~CPU_SIGN_BIT;

  switch(irq) {
    case IRQ_MACHINE_TMR_INT: {
      aclint_setMtimecmp(hart->aclint, (int)hart->csr.mhartid, CPU_UINT_MAX);
      hart->csr.mie &= ~((cpu_word_t)1 << (unsigned int)IRQ_MACHINE_TMR_INT);

      rv_setInterrupt(hart, IRQ_SUPERVISOR_TMR_INT);
      break;
    }

    default: {
      ERROR("SBI: Unexpected M-mode interrupt: %" PRI_CPU_UWORD, irq);
      assert(false);
      break;
    }
  }
}

void sbi_handleException(RV_Cpu *hart) {
  ERROR("SBI: M-mode exception: %" PRI_CPU_UWORD, hart->csr.mcause);
  assert(false);
}
