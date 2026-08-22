//
// Created by palulukan on 7/24/26.
//

#include "rv64.h"

#include "log.h"
#include "utils.h"

#include "rv_cext.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef CONFIG_DOS
// static bool _trace = false;
#endif

static inline void _trap(RV64_Cpu *self, RV64_MCAUSE cause, bool interrupt) {
  self->trap = true;

  self->mcause = cause;
  if(interrupt)
    self->mcause |= CPU_SIGN_BIT;
}

static inline void _flushIcache(RV64_Cpu *self) {
  self->icache.base = CPU_UINT_MAX;
}

static inline void _processPendingInterrupts(RV64_Cpu *self) {
  if(!(self->mstatus & MSTATUS_MIE_MASK))
    return;

  const unsigned int vectors[] = {
    MCAUSE_MACHINE_EXT_INT,
    MCAUSE_MACHINE_SW_INT,
    MCAUSE_MACHINE_TMR_INT,
    // MCAUSE_SUPERVISOR_EXT_INT,
    // MCAUSE_SUPERVISOR_SW_INT,
    // MCAUSE_SUPERVISOR_TMR_INT,
    // MCAUSE_CTR_OVF_INT,
  };

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

static inline cpu_word_t _readReg(const RV64_Cpu *self, unsigned int rd) {
  return self->regs.Rx[rd];
}

static inline void _writeReg(RV64_Cpu *self, unsigned int rd, cpu_word_t d) {
  if(!rd || self->trap)
    return;

  self->regs.Rx[rd] = d;
}

static inline cpu_word_t _readPC(const RV64_Cpu *self) {
  return self->PC;
}

static inline void _writePC(RV64_Cpu *self, cpu_word_t d) {
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

#ifndef CONFIG_DOS
  // if(_trace) {
  //   char *s = malloc(size * 4 + 2 + 1);
  //   char *p = s;
  //   for(size_t i = 0; i < size; ++i)
  //     p += sprintf(p, "%02x ", ((const uint8_t *)buf)[i]);
  //   p += sprintf(p, "| ");
  //   for(size_t i = 0; i < size; ++i)
  //     *p++ = isprint(((const char *)buf)[i]) ? ((const char *)buf)[i] : '.';
  //   *p = '\0';
  //   DEBUG("Mem RD: %04lu [%s] <- %" PRI_CPU_PTR, size, s, addr);
  //   free(s);
  // }
#endif

}

static inline void _writeMem(RV64_Cpu *self, cpu_addr_t addr, const void* buf, size_t size) {
  if(self->trap)
    return;

#ifndef CONFIG_DOS
  // if(_trace || addr == 0x807917b8) {
  //   char *s = malloc(size * 4 + 2 + 1);
  //   char *p = s;
  //   for(size_t i = 0; i < size; ++i)
  //     p += sprintf(p, "%02x ", ((const uint8_t *)buf)[i]);
  //   p += sprintf(p, "| ");
  //   for(size_t i = 0; i < size; ++i)
  //     *p++ = isprint(((const char *)buf)[i]) ? ((const char *)buf)[i] : '.';
  //   *p = '\0';
  //   DEBUG("Mem WR: %04lu [%s] -> %" PRI_CPU_PTR, size, s, addr);
  //   free(s);
  // }
#endif

  if(!bus_write(self->bus, addr, buf, size))
    _trap(self, MCAUSE_ST_AF, false);
}

// static inline uint32_t _readMem32(RV64_Cpu *self, cpu_addr_t addr) {
//   uint32_t res = 0;
//   _readMem(self, addr, &res, sizeof(res));
//   return res;
// }

// static inline void _writeMem32(RV64_Cpu *self, cpu_addr_t addr, uint32_t data) {
//   _writeMem(self, addr, &data, sizeof(data));
// }

// static inline uint64_t _readMem64(RV64_Cpu *self, cpu_addr_t addr) {
//   uint64_t res = 0;
//   _readMem(self, addr, &res, sizeof(res));
//   return res;
// }

// static inline void _writeMem64(RV64_Cpu *self, cpu_addr_t addr, uint64_t data) {
//   _writeMem(self, addr, &data, sizeof(data));
// }

static inline uint32_t _fetch(RV64_Cpu *self) {
  const cpu_word_t pc = _readPC(self);

  if(pc & 1) {
    _trap(self, MCAUSE_INST_ALLIGN, false);
    return 0;
  }

  uint32_t res = 0;

  cpu_addr_t offset = pc - self->icache.base;
  if(self->icache.base == CPU_UINT_MAX || offset > (ICACHE_LINE_SIZE - sizeof(res))) {
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
  }

  memcpy(&res, self->icache.line + offset, sizeof(res));
  return res;
}

static inline void _decode(uint32_t instr, struct _instr *res) {
  if(!rv_decodeCext(instr, res)) {
    // DEBUG("RV64: decoding: 0x%08" PRIx32, instr);

    res->size = 4;
    res->instr = instr;

    res->opcode = instr & 0x7F;

    res->rd = (instr >> 7) & 0x1F;
    res->rs1 = (instr >> 15) & 0x1F;
    res->rs2 = (instr >> 20) & 0x1F;

    res->funct3 = (instr >> 12) & 0x7;
    res->funct5 = (instr >> 27) & 0x1F;
    res->funct7 = (instr >> 25) & 0x7F;
    res->funct12 = (instr >> 20) & 0xFFF;

    // res->aq = !!(instr & (1LU << 26));
    // res->rl = !!(instr & (1LU << 25));

    res->jimm = (instr & (0xFFLU << 12)) | ((instr >> 20) & (0x3FFLU << 1)) | ((instr >> 9) & (1LU << 11)) | ((instr >> 11) & (1LU << 20));
    res->iimm = (instr >> 20) & 0xFFF;
    res->bimm = ((instr >> 7) & (0xFU << 1)) | ((instr >> 20) & (0x3FU << 5)) | ((instr << 4) & (1U << 11)) | ((instr >> 19) & (1U << 12));
    res->uimm = instr & 0xFFFFF000LU;
    res->simm = ((instr >> 7) & 0x1F) | ((instr >> 20) & (0x7FU << 5));
  }
}

static inline void _doJAL(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->jimm, 20, cpu_word_t);
  const cpu_word_t pc = _readPC(self);
  const cpu_word_t target = pc + imm;

  _writeReg(self, di->rd, pc + di->size);
  _writePC(self, target - di->size);
}

static inline void _doSYSTEM(RV64_Cpu* self, const struct _instr *di) {
  switch(di->funct3) {
    case 0: {
      switch(di->iimm) {
        case 0: {
          // ECALL
#ifndef CONFIG_DOS
          // cpu_word_t syscall = self->regs.abi.a7;
          // if(syscall == 222) {
          //   // mmap
          //   cpu_word_t addr = self->regs.abi.a0;
          //   cpu_word_t len = self->regs.abi.a1;
          //   if(len > 4096)
          //     DEBUG("mmap: %" PRI_CPU_SIZE, len);
          //   // _trace = true;
          // }
#endif
          RV64_MCAUSE cause;
          switch(self->mode) {
            default:
            case RV64_PRIV_MODE_USER: cause = MCAUSE_CALL_U; break;
            case RV64_PRIV_MODE_SUPERVISOR: cause = MCAUSE_CALL_S; break;
            case RV64_PRIV_MODE_MACHINE: cause = MCAUSE_CALL_M; break;
          }

          self->mtval = 0;
          _trap(self, cause, false);
          break;
        }

        case 0x105: {
          // WFI
          self->wfi = true;
          break;
        }

        case 0x302: {
          // MRET
          // Restore MIE
          self->mstatus = (self->mstatus & ~MSTATUS_MIE_MASK) | ((self->mstatus & MSTATUS_MPIE_MASK) ? MSTATUS_MIE_MASK : 0);
          // Restore privilege mode
          self->mode = (self->mstatus >> 11) & 3;
          self->mstatus = (self->mstatus & ~MSTATUS_MPP_MASK) | ((cpu_word_t)RV64_PRIV_MODE_USER << 11);
          _writePC(self, self->mepc - di->size);
          break;
        }

        default:
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
      }
      break;
    }

    case 1: {
      // CSRRW
      const cpu_word_t data = _readReg(self, di->rs1);
      if(di->rd)
        _writeReg(self, di->rd, _readCSR(self, di->iimm));
      _writeCSR(self, di->iimm, data);
      break;
    }

    case 2: {
      // CSRRS
      const cpu_word_t data = _readReg(self, di->rs1);
      const cpu_word_t csr = _readCSR(self, di->iimm);
      _writeReg(self, di->rd, csr);
      if(di->rs1)
        _writeCSR(self, di->iimm, csr | data);
      break;
    }

    case 3: {
      // CSRRC
      const cpu_word_t data = _readReg(self, di->rs1);
      const cpu_word_t csr = _readCSR(self, di->iimm);
      _writeReg(self, di->rd, csr);
      if(di->rs1)
        _writeCSR(self, di->iimm, csr & ~data);
      break;
    }

    case 5: {
      // CSRRWI
      if(di->rd)
        _writeReg(self, di->rd, _readCSR(self, di->iimm));
      _writeCSR(self, di->iimm, di->rs1);
      break;
    }

    case 6: {
      // CSRRSI
      const cpu_word_t csr = _readCSR(self, di->iimm);
      _writeReg(self, di->rd, csr);
      _writeCSR(self, di->iimm, csr | di->rs1);
      break;
    }

    case 7: {
      // CSRRCI
      const cpu_word_t csr = _readCSR(self, di->iimm);
      _writeReg(self, di->rd, csr);
      _writeCSR(self, di->iimm, csr & ~(cpu_word_t)di->rs1);
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }
}

static inline void _doMISCMEM(RV64_Cpu* self, const struct _instr *di) {
  switch(di->funct3) {
    case 1: {
      if(!di->funct12 && !di->rs1 && !di->rd) {
        // FENCE.I
        _flushIcache(self);
      } else {
        _trap(self, MCAUSE_INST_ILL, false);
        assert(false);
      }
      break;
    }

    default: {
      // _trap(self, MCAUSE_INST_ILL, false);
      // assert(false);
      break;
    }
  }
}

static inline void _doOPIMM(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->iimm, 11, cpu_word_t);
  const cpu_word_t rs1 = _readReg(self, di->rs1);

  switch(di->funct3) {
    // ADDI
    default:
    case 0: _writeReg(self, di->rd, imm + rs1); break;

    case 1: {
      const cpu_word_t f = di->iimm >> 6;
      if(f == 0) {
        // SLLI
        _writeReg(self, di->rd, rs1 << (di->iimm & 0x3F));
      } else {
        _trap(self, MCAUSE_INST_ILL, false);
        assert(false);
      }
      break;
    }

    // SLTI
    case 2: _writeReg(self, di->rd, (cpu_sword_t)rs1 < (cpu_sword_t)imm); break;

    // SLTIU
    case 3: _writeReg(self, di->rd, rs1 < imm); break;

    // XORI
    case 4: _writeReg(self, di->rd, rs1 ^ imm); break;

    case 5: {
      const unsigned int shift = di->iimm & 0x3F;
      switch(di->iimm >> 6) {
        // SRLI
        case 0: _writeReg(self, di->rd, rs1 >> shift); break;

        case 0x10: {
          // SRAI
          const cpu_word_t sign = ~(((rs1 & CPU_SIGN_BIT) >> shift) - 1);
          _writeReg(self, di->rd, (rs1 >> shift) | sign);
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    // ORI
    case 6: _writeReg(self, di->rd, imm | rs1); break;

    // ANDI
    case 7: _writeReg(self, di->rd, imm & rs1); break;
  }
}

static inline void _doBRANCH(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->bimm, 12, cpu_word_t);
  const cpu_word_t rs1 = _readReg(self, di->rs1);
  const cpu_word_t rs2 = _readReg(self, di->rs2);

  switch(di->funct3) {
    case 0: {
      // BEQ
      if(rs1 == rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    case 1: {
      // BNE
      if(rs1 != rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    case 4: {
      // BLT
      if((cpu_sword_t)rs1 < (cpu_sword_t)rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    case 5: {
      // BGE
      if((cpu_sword_t)rs1 >= (cpu_sword_t)rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    case 6: {
      // BLTU
      if(rs1 < rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    case 7: {
      // BGEU
      if(rs1 >= rs2)
        _writePC(self, _readPC(self) + imm - di->size);
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }
}

static inline void _doLUI(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->uimm, 31, cpu_word_t);

  _writeReg(self, di->rd, imm);
}

static inline void _doOP(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t rs1 = _readReg(self, di->rs1);
  const cpu_word_t rs2 = _readReg(self, di->rs2);

  switch(di->funct3) {
    default:
    case 0: {
      switch(di->funct7) {
        // ADD
        case 0: _writeReg(self, di->rd, rs1 + rs2); break;

        // MUL
        case 1: _writeReg(self, di->rd, rs1 * rs2); break;

        // SUB
        case 32: _writeReg(self, di->rd, rs1 - rs2); break;

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 1: {
      switch(di->funct7) {
        // SLL
        case 0: _writeReg(self, di->rd, rs1 << (rs2 & 0x3F)); break;

        // MULH
        case 1: {
          // Ai generated :-)
          const uint64_t ua = rs1;
          const uint64_t ub = rs2;

          const uint64_t ma = ((cpu_sword_t)rs1 < 0) ? (0 - ua) : ua;
          const uint64_t mb = ((cpu_sword_t)rs2 < 0) ? (0 - ub) : ub;

          const uint64_t a0 = (uint32_t)ma;
          const uint64_t a1 = ma >> 32;
          const uint64_t b0 = (uint32_t)mb;
          const uint64_t b1 = mb >> 32;

          const uint64_t p0 = a0 * b0;
          const uint64_t p1 = a0 * b1;
          const uint64_t p2 = a1 * b0;
          const uint64_t p3 = a1 * b1;

          const uint64_t mid = (p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;

          const uint64_t lo = (p0 & UINT64_C(0xffffffff)) | (mid << 32);
          uint64_t hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);

          if (((cpu_sword_t)rs1 < 0) != ((cpu_sword_t)rs2 < 0))
            hi = ~hi + (lo == 0);

          _writeReg(self, di->rd, hi);
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 2: {
      if(di->funct7 == 0) {
        // SLT
        _writeReg(self, di->rd, (cpu_sword_t)rs1 < (cpu_sword_t)rs2);
      } else {
        _trap(self, MCAUSE_INST_ILL, false);
        assert(false);
      }
      break;
    }

    case 3: {
      switch(di->funct7) {
        // SLTU
        case 0: _writeReg(self, di->rd, rs1 < rs2); break;

        // MULHU
        case 1: {
          // Ai generated :-)
          const uint64_t rs1L = (uint32_t)rs1;
          const uint64_t rs1H = rs1 >> 32;
          const uint64_t rs2L = (uint32_t)rs2;
          const uint64_t rs2H = rs2 >> 32;

          const uint64_t p0 = rs1L * rs2L;
          const uint64_t p1 = rs1L * rs2H;
          const uint64_t p2 = rs1H * rs2L;
          const uint64_t p3 = rs1H * rs2H;

          const uint64_t carry = (p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;
          const uint64_t res = p3 + (p1 >> 32) + (p2 >> 32) + (carry >> 32);

          _writeReg(self, di->rd, res);
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 4: {
      switch(di->funct7) {
        // XOR
        case 0: _writeReg(self, di->rd, rs1 ^ rs2); break;

        case 1: {
          // DIV
          if(rs2) {
            if(rs1 == CPU_SIGN_BIT && rs2 == CPU_UINT_MAX)
              _writeReg(self, di->rd, CPU_SIGN_BIT);
            else
              _writeReg(self, di->rd, (cpu_sword_t)rs1 / (cpu_sword_t)rs2);
          } else {
            _writeReg(self, di->rd, CPU_UINT_MAX);
          }
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 5: {
      switch(di->funct7) {
        // SRL
        case 0: _writeReg(self, di->rd, rs1 >> (rs2 & 0x3F)); break;

        case 1: {
          // DIVU
          if(rs2)
            _writeReg(self, di->rd, rs1 / rs2);
          else
            _writeReg(self, di->rd, CPU_UINT_MAX);
          break;
        }

        case 32: {
          // SRA
          const unsigned int shift = rs2 & 0x3F;
          const cpu_word_t sign = ~(((rs1 & CPU_SIGN_BIT) >> shift) - 1);
          _writeReg(self, di->rd, (rs1 >> shift) | sign);
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 6: {
      switch(di->funct7) {
        // OR
        case 0: _writeReg(self, di->rd, rs1 | rs2); break;

        case 1: {
          // REM
          if(rs2) {
            if(rs1 == CPU_SIGN_BIT && rs2 == CPU_UINT_MAX)
              _writeReg(self, di->rd, 0);
            else
              _writeReg(self, di->rd, (cpu_sword_t)rs1 % (cpu_sword_t)rs2);
          } else {
            _writeReg(self, di->rd, rs1);
          }
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 7: {
      switch(di->funct7) {
        // AND
        case 0: _writeReg(self, di->rd, rs1 & rs2); break;

        case 1: {
          // REMU
          if(rs2)
            _writeReg(self, di->rd, rs1 % rs2);
          else
            _writeReg(self, di->rd, rs1);
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }
  }
}

static inline void _doJALR(RV64_Cpu* self, const struct _instr *di) {
  switch(di->funct3) {
    case 0: {
      // JALR
      const cpu_word_t imm = SIGN_EXTEND(di->iimm, 11, cpu_word_t);
      const cpu_word_t target = (_readReg(self, di->rs1) + imm) & ~(cpu_word_t)1;

      _writeReg(self, di->rd, _readPC(self) + di->size);
      _writePC(self, target - di->size);
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }
}

static inline void _doAUIPC(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->uimm, 31, cpu_word_t);

  _writeReg(self, di->rd, _readPC(self) + imm);
}

static inline void _doOPIMM32(RV64_Cpu* self, const struct _instr *di) {
  const cpu_word_t imm = SIGN_EXTEND(di->iimm, 11, cpu_word_t);
  const uint32_t rs1 = _readReg(self, di->rs1);

  switch(di->funct3) {
    // ADDIW
    case 0: {
      const uint32_t res = imm + rs1;
      _writeReg(self, di->rd, SIGN_EXTEND(res, 31, cpu_word_t));
      break;
    }

    case 1: {
      const cpu_word_t f = di->iimm >> 5;
      if(f == 0) {
        // SLLIW
        const uint32_t res = rs1 << (di->iimm & 0x1F);
        _writeReg(self, di->rd, SIGN_EXTEND(res, 31, cpu_word_t));
      } else {
        _trap(self, MCAUSE_INST_ILL, false);
        assert(false);
      }
      break;
    }

    case 5: {
      const unsigned int shift = di->iimm & 0x1F;
      switch(di->iimm >> 5) {
        case 0: {
          // SRLIW
          const uint32_t res = rs1 >> shift;
          _writeReg(self, di->rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        case 0x20: {
          // SRAIW
          const uint32_t sign = ~(((rs1 & ((uint32_t)1 << ((sizeof(uint32_t) * 8) - 1))) >> shift) - 1);
          const uint32_t res = (rs1 >> shift) | sign;
          _writeReg(self, di->rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }
}

static inline void _doAMO(RV64_Cpu* self, const struct _instr *di) {
  const size_t size = 1 << di->funct3;
  if(size != 4 && size != 8) {
    _trap(self, MCAUSE_INST_ILL, false);
    return;
  }

  const cpu_addr_t rs1 = _readReg(self, di->rs1);
  if((rs1 & (size - 1))) {
    _trap(self, MCAUSE_ST_ALLIGN, false);
    return;
  }

  const cpu_word_t rs2 = _readReg(self, di->rs2);

  cpu_word_t data = 0;
  _readMem(self, rs1, &data, size);
  data = SIGN_EXTEND(data, (size * 8) - 1, cpu_word_t);

  bool needWrite = true;
  cpu_word_t rd = 0;

  switch(di->funct5) {
    case 0: {
      // AMOADD.x
      rd = data;
      data += rs2;
      break;
    }

    case 1: {
      // AMOSWAP.x
      rd = data;
      data = rs2;
      break;
    }

    case 2: {
      // LR.x
      if(di->rs2) {
        _trap(self, MCAUSE_INST_ILL, false);
        break;
      }

      rd = data;
      bus_reservationCreate(self->bus, rs1, size);
      needWrite = false;
      break;
    }

    case 3: {
      // SC.x
      if(bus_reservationCheckInvalidate(self->bus, rs1, size)) {
        rd = 0;
        data = rs2;
      } else {
        rd = 1;
        needWrite = false;
      }
      break;
    }

    case 4: {
      // AMOXOR.x
      rd = data;
      data ^= rs2;
      break;
    }

    case 8: {
      // AMOOR.x
      rd = data;
      data |= rs2;
      break;
    }

    case 12: {
      // AMOAND.x
      rd = data;
      data &= rs2;
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }

  _writeReg(self, di->rd, rd);

  if(needWrite)
    _writeMem(self, rs1, &data, size);
}

static inline void _doSTORE(RV64_Cpu* self, const struct _instr *di) {
  const size_t size = 1 << di->funct3;
  if(size > 8) {
    _trap(self, MCAUSE_INST_ILL, false);
    return;
  }

  const cpu_word_t offset = SIGN_EXTEND(di->simm, 11, cpu_word_t);
  const cpu_word_t addr = _readReg(self, di->rs1) + offset;

  const cpu_word_t data = _readReg(self, di->rs2);
  _writeMem(self, addr, &data, size);
}

static inline void _doLOAD(RV64_Cpu* self, const struct _instr *di) {
  const size_t size = 1 << (di->funct3 & 3U);
  const bool isSigned = !(di->funct3 & 4U);

  const cpu_word_t offset = SIGN_EXTEND(di->iimm, 11, cpu_word_t);
  const cpu_word_t addr = _readReg(self, di->rs1) + offset;

  cpu_word_t data = 0;
  _readMem(self, addr, &data, size);
  if(isSigned)
    data = SIGN_EXTEND(data, (size * 8) - 1, cpu_word_t);

  _writeReg(self, di->rd, data);
}

static inline void _doOP32(RV64_Cpu* self, const struct _instr *di) {
  const uint32_t rs1 = _readReg(self, di->rs1);
  const uint32_t rs2 = _readReg(self, di->rs2);

  switch(di->funct3) {
    case 0: {
      switch(di->funct7) {
        // ADDW
        case 0: _writeReg(self, di->rd, SIGN_EXTEND(rs1 + rs2, 31, cpu_word_t)); break;

        // MULW
        case 1: _writeReg(self, di->rd, SIGN_EXTEND(rs1 * rs2, 31, cpu_word_t)); break;

        // SUBW
        case 32: _writeReg(self, di->rd, SIGN_EXTEND(rs1 - rs2, 31, cpu_word_t)); break;

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 1: {
      switch(di->funct7) {
        // SLLW
        case 0: _writeReg(self, di->rd, SIGN_EXTEND(rs1 << (rs2 & 0x3F), 31, cpu_word_t)); break;

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 4: {
      switch(di->funct7) {
        case 1: {
          // DIVW
          if(rs2) {
            if(rs1 == INT32_MIN && rs2 == UINT32_MAX)
              _writeReg(self, di->rd, SIGN_EXTEND(INT32_MIN, 31, cpu_word_t));
            else
              _writeReg(self, di->rd, SIGN_EXTEND((int32_t)rs1 / (int32_t)rs2, 31, cpu_word_t));
          } else {
            _writeReg(self, di->rd, CPU_UINT_MAX);
          }
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 5: {
      switch(di->funct7) {
        // SRLW
        case 0: _writeReg(self, di->rd, SIGN_EXTEND(rs1 >> (rs2 & 0x1F), 31, cpu_word_t)); break;

        case 1: {
          // DIVUW
          if(rs2)
              _writeReg(self, di->rd, SIGN_EXTEND(rs1 / rs2, 31, cpu_word_t));
          else
            _writeReg(self, di->rd, CPU_UINT_MAX);
          break;
        }

        case 32: {
          // SRAW
          const unsigned int shift = rs2 & 0x1F;
          const uint32_t sign = ~(((rs1 & ((uint32_t)1 << ((sizeof(uint32_t) * 8) - 1))) >> shift) - 1);
          const uint32_t res = (rs1 >> shift) | sign;
          _writeReg(self, di->rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 6: {
      switch(di->funct7) {
        case 1: {
          // REMW
          if(rs2) {
            if(rs1 == INT32_MIN && rs2 == UINT32_MAX)
              _writeReg(self, di->rd, 0);
            else
              _writeReg(self, di->rd, SIGN_EXTEND((int32_t)rs1 % (int32_t)rs2, 31, cpu_word_t));
          } else {
            _writeReg(self, di->rd, SIGN_EXTEND(rs1, 31, cpu_word_t));
          }
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    case 7: {
      switch(di->funct7) {
        case 1: {
          // REMUW
          if(rs2)
            _writeReg(self, di->rd, SIGN_EXTEND(rs1 % rs2, 31, cpu_word_t));
          else
            _writeReg(self, di->rd, SIGN_EXTEND(rs1, 31, cpu_word_t));
          break;
        }

        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }
      }
      break;
    }

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }
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
}

void rv64_reset(RV64_Cpu *self, cpu_addr_t start) {
  _flushIcache(self);

  bus_reservationCheckInvalidate(self->bus, 0, 0);

#ifndef CONFIG_DOS
  self->cycle = 0;
#endif

  self->wfi = false;

  self->mstatus = MSTATUS_WR_VAL(0U);

  self->trap = false;

  self->mode = RV64_PRIV_MODE_MACHINE;

  _writePC(self, start);
}

void rv64_run(RV64_Cpu *self) {
#ifndef CONFIG_DOS
  ++self->cycle;

  // _trace = true;
  // if(self->cycle == 428904) {
  //   bus_dump(self->bus, self->regs.abi.a0, 1024);
  //   // bus_dump(self->bus, self->regs.abi.a1, 1024);
  // }
  // if(self->PC == 0x801c017a) {
  //   bus_dump(self->bus, 0x80307c80, 8);
  //   // bus_dump(self->bus, self->regs.abi.a1, 128);
  // }
  // if(self->cycle >= 428904)
  //   _trace = true;
  // if(self->PC == 0x801c025e)
  // if(_trace || self->cycle >= 2201990 || self->PC == 0x80095ed8)
  //   _trace = true;
  // if(_trace || self->PC == 0x80227904)
  // if(_trace || self->cycle >= 18593005) {
  //   // bus_dump(self->bus, self->mepc - 4, 1024);
  //   _trace = true;
  // }

  assert(self->regs.Rx[CPU_REG_X0] == 0);
#endif

  // if(self->cycle >= 1982466) {
  //   // bus_dump(self->bus, self->regs.abi.a0, 128);
  //   DEBUG("PC: %" PRI_CPU_PTR " A6: %" PRI_CPU_PTR " A5: %" PRI_CPU_PTR " A4: %" PRI_CPU_PTR " A2: %" PRI_CPU_PTR, self->PC, self->regs.abi.a6, self->regs.abi.a5, self->regs.abi.a4, self->regs.abi.a2);
  // }

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

#ifndef CONFIG_DOS
  // if(_trace)
  //   DEBUG("RV64: %" PRI_CPU_PTR ": 0x%08" PRIx32, _readPC(self), instr);
#endif

  struct _instr di;
  _decode(instr, &di);

  switch(di.opcode) {
    case 0x6F: _doJAL(self, &di); break;
    case 0x73: _doSYSTEM(self, &di); break;
    case 0x0F: _doMISCMEM(self, &di); break;
    case 0x13: _doOPIMM(self, &di); break;
    case 0x63: _doBRANCH(self, &di); break;
    case 0x37: _doLUI(self, &di); break;
    case 0x33: _doOP(self, &di); break;
    case 0x67: _doJALR(self, &di); break;
    case 0x17: _doAUIPC(self, &di); break;
    case 0x1B: _doOPIMM32(self, &di); break;
    case 0x2F: _doAMO(self, &di); break;
    case 0x23: _doSTORE(self, &di); break;
    case 0x03: _doLOAD(self, &di); break;
    case 0x3B: _doOP32(self, &di); break;

    default: {
      _trap(self, MCAUSE_INST_ILL, false);
      assert(false);
      break;
    }
  }

  _writePC(self, _readPC(self) + di.size);
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
