//
// Created by palulukan on 8/30/26.
//

#ifndef RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD
#define RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD

#include "rv.h"

#include "rv_internal.h"
#include "rv_csr.h"

#include "../utils.h"

#include <assert.h>

static inline void _doILL(RV_Cpu* self) {
  _trap(self, TRAP_INST_ILL);

  DEBUG("RV: ILL PC: %" PRI_CPU_PTR, _readPC(self));
  assert(false);
}

static inline void _doJAL(RV_Cpu* self, uint32_t jimm, unsigned int rd, int size) {
  const cpu_word_t imm = SIGN_EXTEND(jimm, 20, cpu_word_t);
  const cpu_addr_t pc = _readPC(self);
  const cpu_word_t target = pc + imm;

  _writeReg(self, rd, pc + size);
  _writePC(self, target - size);
}

static inline void _doSYSTEM(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1, int size) {
  const uint32_t funct12 = iimm;

  switch(funct3) {
    case 0: {
      switch(funct12) {
        case 0: {
          // ECALL
          RV_TrapCause cause;
          switch(self->mode) {
            default:
            case RV_PRIV_MODE_USER: cause = TRAP_CALL_U; break;
            case RV_PRIV_MODE_SUPERVISOR: cause = TRAP_CALL_S; break;
            case RV_PRIV_MODE_MACHINE: cause = TRAP_CALL_M; break;
          }

          _trap(self, cause);
          break;
        }

        case 1: {
          // EBREAK
          _trap(self, TRAP_BREAKPOINT);
          break;
        }

        case 0x102: {
          // SRET
          _xret(self, RV_PRIV_MODE_SUPERVISOR, size);
          break;
        }

        case 0x105: {
          // WFI
          if((self->mode < RV_PRIV_MODE_MACHINE && (self->csr.mstatus & MSTATUS_TW_MASK)) || (self->mode == RV_PRIV_MODE_USER)) {
            _trap(self, TRAP_INST_ILL);
            break;
          }

          self->wfi = true;
          break;
        }

        case 0x302: {
          // MRET
          _xret(self, RV_PRIV_MODE_MACHINE, size);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 1: {
      // CSRRW
      const cpu_word_t data = _readReg(self, rs1);
      const cpu_word_t csr = _readCSR(self, iimm);
      _writeCSR(self, iimm, data);
      if(rd)
        _writeReg(self, rd, csr);
      break;
    }

    case 2: {
      // CSRRS
      const cpu_word_t data = _readReg(self, rs1);
      const cpu_word_t csr = _readCSR(self, iimm);
      if(rs1)
        _writeCSR(self, iimm, csr | data);
      _writeReg(self, rd, csr);
      break;
    }

    case 3: {
      // CSRRC
      const cpu_word_t data = _readReg(self, rs1);
      const cpu_word_t csr = _readCSR(self, iimm);
      if(rs1)
        _writeCSR(self, iimm, csr & ~data);
      _writeReg(self, rd, csr);
      break;
    }

    case 5: {
      // CSRRWI
      const cpu_word_t csr = _readCSR(self, iimm);
      _writeCSR(self, iimm, rs1);
      if(rd)
        _writeReg(self, rd, csr);
      break;
    }

    case 6: {
      // CSRRSI
      const cpu_word_t csr = _readCSR(self, iimm);
      if(rs1)
        _writeCSR(self, iimm, csr | rs1);
      _writeReg(self, rd, csr);
      break;
    }

    case 7: {
      // CSRRCI
      const cpu_word_t csr = _readCSR(self, iimm);
      if(rs1)
        _writeCSR(self, iimm, csr & ~(cpu_word_t)rs1);
      _writeReg(self, rd, csr);
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

static inline void _doMISCMEM(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1) {
  switch(funct3) {
    // FENCE
    // di->rs1 and di->rd are ignored
    case 0:
    // FENCE.I
    // di->iimm, di->rs1 and di->rd are ignored
    case 1: {
      _flushIcache(self);
      break;
    }

    case 2: {
      // CBO
      switch(iimm) {
        default: {
          _doILL(self);
          break;
        }

        case 4: {
          // CBO.ZERO
          if(!rd) {
            if(self->mode < RV_PRIV_MODE_MACHINE && (self->csr.menvcfg & MENVCFG_CBZE_MASK) == 0) {
              _trap(self, TRAP_INST_ILL);
              break;
            }
            if(self->mode < RV_PRIV_MODE_SUPERVISOR && (self->csr.senvcfg & SENVCFG_CBZE_MASK) == 0) {
              _trap(self, TRAP_INST_ILL);
              break;
            }

            static const uint8_t zero[DCACHE_LINE_SIZE] = {0};

            const cpu_addr_t mask = ~(cpu_addr_t)(DCACHE_LINE_SIZE - 1);
            _writeMem(self, _readReg(self, rs1) & mask, zero, sizeof(zero));
          } else {
            _doILL(self);
          }
          break;
        }
      }
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

static inline void _doOPIMM(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1) {
  const cpu_word_t imm = SIGN_EXTEND(iimm, 11, cpu_word_t);
  const cpu_word_t rs1Val = _readReg(self, rs1);

  switch(funct3) {
    default:
    // ADDI
    case 0: _writeReg(self, rd, imm + rs1Val); break;

    case 1: {
      switch(iimm >> 6) {
        // SLLI
        case 0: _writeReg(self, rd, rs1Val << (iimm & CPU_SHIFT_MASK)); break;

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    // SLTI
    case 2: _writeReg(self, rd, (cpu_sword_t)rs1Val < (cpu_sword_t)imm); break;

    // SLTIU
    case 3: _writeReg(self, rd, rs1Val < imm); break;

    // XORI
    case 4: _writeReg(self, rd, rs1Val ^ imm); break;

    case 5: {
      const unsigned int shift = iimm & CPU_SHIFT_MASK;
      switch(iimm >> 6) {
        // SRLI
        case 0: _writeReg(self, rd, rs1Val >> shift); break;

        case 0x10: {
          // SRAI
          const cpu_word_t sign = ~(((rs1Val & CPU_SIGN_BIT) >> shift) - 1);
          _writeReg(self, rd, (rs1Val >> shift) | sign);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    // ORI
    case 6: _writeReg(self, rd, imm | rs1Val); break;

    // ANDI
    case 7: _writeReg(self, rd, imm & rs1Val); break;
  }
}

static inline void _doBRANCH(RV_Cpu* self, unsigned int funct3, uint32_t bimm, unsigned int rs1, unsigned int rs2, int size) {
  const cpu_word_t imm = SIGN_EXTEND(bimm, 12, cpu_word_t);
  const cpu_word_t rs1Val = _readReg(self, rs1);
  const cpu_word_t rs2Val = _readReg(self, rs2);

  switch(funct3) {
    case 0: {
      // BEQ
      if(rs1Val == rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    case 1: {
      // BNE
      if(rs1Val != rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    case 4: {
      // BLT
      if((cpu_sword_t)rs1Val < (cpu_sword_t)rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    case 5: {
      // BGE
      if((cpu_sword_t)rs1Val >= (cpu_sword_t)rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    case 6: {
      // BLTU
      if(rs1Val < rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    case 7: {
      // BGEU
      if(rs1Val >= rs2Val)
        _writePC(self, _readPC(self) + imm - size);
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

static inline void _doLUI(RV_Cpu* self, uint32_t uimm, unsigned int rd) {
#ifdef CONFIG_RV64
  const cpu_word_t imm = SIGN_EXTEND(uimm, 31, cpu_word_t);
#else
  const cpu_word_t imm = uimm;
#endif

  _writeReg(self, rd, imm);
}

static inline void _doOP(RV_Cpu* self, unsigned int funct3, unsigned int funct7, unsigned int rd, unsigned int rs1, unsigned int rs2) {
  const cpu_word_t rs1Val = _readReg(self, rs1);
  const cpu_word_t rs2Val = _readReg(self, rs2);

  switch(funct3) {
    default:
    case 0: {
      switch(funct7) {
        // ADD
        case 0: _writeReg(self, rd, rs1Val + rs2Val); break;

        // MUL
        case 1: _writeReg(self, rd, rs1Val * rs2Val); break;

        // SUB
        case 32: _writeReg(self, rd, rs1Val - rs2Val); break;

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 1: {
      switch(funct7) {
        // SLL
        case 0: _writeReg(self, rd, rs1Val << (rs2Val & CPU_SHIFT_MASK)); break;

        case 1: {
          // MULH
#ifdef CONFIG_RV64
          // Ai generated :-)
          const uint64_t ua = rs1Val;
          const uint64_t ub = rs2Val;

          const uint64_t ma = ((cpu_sword_t)rs1Val < 0) ? (0 - ua) : ua;
          const uint64_t mb = ((cpu_sword_t)rs2Val < 0) ? (0 - ub) : ub;

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

          if (((cpu_sword_t)rs1Val < 0) != ((cpu_sword_t)rs2Val < 0))
            hi = ~hi + (lo == 0);
#else
          const uint64_t p = (int64_t)((int32_t)rs1Val) * (int64_t)((int32_t)rs2Val);
          const uint32_t hi = p >> 32;
#endif
          _writeReg(self, rd, hi);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 2: {
      switch(funct7) {
        // SLT
        case 0: _writeReg(self, rd, (cpu_sword_t)rs1Val < (cpu_sword_t)rs2Val); break;

        case 1: {
          // MULHSU
#ifdef CONFIG_RV64
          // Ai generated :-)
          const uint64_t ua = rs1Val;
          const uint64_t ub = rs2Val;

          const uint64_t ma = ((cpu_sword_t)rs1Val < 0) ? (0 - ua) : ua;
          const uint64_t mb = ub;

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

          if ((cpu_sword_t)rs1Val < 0)
            hi = ~hi + (lo == 0);
#else
          const uint64_t p = (int64_t)((int32_t)rs1Val) * (uint64_t)rs2Val;
          const uint32_t hi = p >> 32;
#endif
          _writeReg(self, rd, hi);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 3: {
      switch(funct7) {
        // SLTU
        case 0: _writeReg(self, rd, rs1Val < rs2Val); break;

        case 1: {
          // MULHU
#ifdef CONFIG_RV64
          // Ai generated :-)
          const uint64_t rs1L = (uint32_t)rs1Val;
          const uint64_t rs1H = rs1Val >> 32;
          const uint64_t rs2L = (uint32_t)rs2Val;
          const uint64_t rs2H = rs2Val >> 32;

          const uint64_t p0 = rs1L * rs2L;
          const uint64_t p1 = rs1L * rs2H;
          const uint64_t p2 = rs1H * rs2L;
          const uint64_t p3 = rs1H * rs2H;

          const uint64_t carry = (p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;
          const uint64_t res = p3 + (p1 >> 32) + (p2 >> 32) + (carry >> 32);
#else
          const uint64_t p = (uint64_t)rs1Val * (uint64_t)rs2Val;
          const uint32_t res = p >> 32;
#endif
          _writeReg(self, rd, res);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 4: {
      switch(funct7) {
        // XOR
        case 0: _writeReg(self, rd, rs1Val ^ rs2Val); break;

        case 1: {
          // DIV
          if(rs2Val) {
            if(rs1Val == CPU_SIGN_BIT && rs2Val == CPU_UINT_MAX)
              _writeReg(self, rd, CPU_SIGN_BIT);
            else
              _writeReg(self, rd, (cpu_sword_t)rs1Val / (cpu_sword_t)rs2Val);
          } else {
            _writeReg(self, rd, CPU_UINT_MAX);
          }
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 5: {
      switch(funct7) {
        // SRL
        case 0: _writeReg(self, rd, rs1Val >> (rs2Val & CPU_SHIFT_MASK)); break;

        case 1: {
          // DIVU
          if(rs2Val)
            _writeReg(self, rd, rs1Val / rs2Val);
          else
            _writeReg(self, rd, CPU_UINT_MAX);
          break;
        }

        case 32: {
          // SRA
          const unsigned int shift = rs2Val & CPU_SHIFT_MASK;
          const cpu_word_t sign = ~(((rs1Val & CPU_SIGN_BIT) >> shift) - 1);
          _writeReg(self, rd, (rs1Val >> shift) | sign);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 6: {
      switch(funct7) {
        // OR
        case 0: _writeReg(self, rd, rs1Val | rs2Val); break;

        case 1: {
          // REM
          if(rs2Val) {
            if(rs1Val == CPU_SIGN_BIT && rs2Val == CPU_UINT_MAX)
              _writeReg(self, rd, 0);
            else
              _writeReg(self, rd, (cpu_sword_t)rs1Val % (cpu_sword_t)rs2Val);
          } else {
            _writeReg(self, rd, rs1Val);
          }
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 7: {
      switch(funct7) {
        // AND
        case 0: _writeReg(self, rd, rs1Val & rs2Val); break;

        case 1: {
          // REMU
          if(rs2Val)
            _writeReg(self, rd, rs1Val % rs2Val);
          else
            _writeReg(self, rd, rs1Val);
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }
  }
}

static inline void _doJALR(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1, int size) {
  switch(funct3) {
    case 0: {
      // JALR
      const cpu_word_t imm = SIGN_EXTEND(iimm, 11, cpu_word_t);
      const cpu_addr_t target = (_readReg(self, rs1) + imm) & ~(cpu_word_t)1;

      _writeReg(self, rd, _readPC(self) + size);
      _writePC(self, target - size);
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

static inline void _doAUIPC(RV_Cpu* self, uint32_t uimm, unsigned int rd) {
#ifdef CONFIG_RV64
  const cpu_word_t imm = SIGN_EXTEND(uimm, 31, cpu_word_t);
#else
  const cpu_word_t imm = uimm;
#endif

  _writeReg(self, rd, _readPC(self) + imm);
}

static inline void _doAMO(RV_Cpu* self, unsigned int funct3, unsigned int funct5, unsigned int rd, unsigned int rs1, unsigned int rs2) {
  const size_t size = min_size_t(1 << funct3, sizeof(cpu_word_t));

  const cpu_addr_t rs1Val = _readReg(self, rs1);
  if((rs1Val & (size - 1))) {
    _trap(self, TRAP_ST_ALLIGN);
    return;
  }

  const unsigned int sizeBits = size * 8;
  const cpu_word_t mask = CPU_UINT_MAX >> ((sizeof(cpu_word_t) * 8) - sizeBits);

  const cpu_word_t rs2Val = SIGN_EXTEND(_readReg(self, rs2) & mask, sizeBits - 1, cpu_word_t);

  cpu_word_t data = 0;
  _readMem(self, rs1Val, &data, size);
  data = SIGN_EXTEND(data, sizeBits - 1, cpu_word_t);

  bool needWrite = true;
  cpu_word_t rdVal = 0;

  switch(funct5) {
    case 0: {
      // AMOADD.x
      rdVal = data;
      data += rs2Val;
      break;
    }

    case 1: {
      // AMOSWAP.x
      rdVal = data;
      data = rs2Val;
      break;
    }

    case 2: {
      // LR.x
      if(rs2) {
        _doILL(self);
        break;
      }

      rdVal = data;
      bus_reservationCreate(self->bus, rs1Val, size);
      needWrite = false;
      break;
    }

    case 3: {
      // SC.x
      if(bus_reservationCheckInvalidate(self->bus, rs1Val, size)) {
        rdVal = 0;
        data = rs2Val;
      } else {
        rdVal = 1;
        needWrite = false;
      }
      break;
    }

    case 4: {
      // AMOXOR.x
      rdVal = data;
      data ^= rs2Val;
      break;
    }

    case 8: {
      // AMOOR.x
      rdVal = data;
      data |= rs2Val;
      break;
    }

    case 12: {
      // AMOAND.x
      rdVal = data;
      data &= rs2Val;
      break;
    }

    case 16: {
      // AMOMIN.x
      rdVal = data;
      data = min_cpu_sword_t((cpu_sword_t)rs2Val, (cpu_sword_t)data);
      break;
    }

    case 20: {
      // AMOMAX.x
      rdVal = data;
      data = max_cpu_sword_t((cpu_sword_t)rs2Val, (cpu_sword_t)data);
      break;
    }

    case 24: {
      // AMOMINU.x
      rdVal = data;
      data = min_cpu_word_t(rs2Val, data);
      break;
    }

    case 28: {
      // AMOMAXU.x
      rdVal = data;
      data = max_cpu_word_t(rs2Val, data);
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }

  _writeReg(self, rd, rdVal);

  if(needWrite)
    _writeMem(self, rs1Val, &data, size);
}

static inline void _doSTORE(RV_Cpu* self, unsigned int funct3, uint32_t simm, unsigned int rs1, unsigned int rs2) {
  const size_t size = 1 << funct3;
  if(size > sizeof(cpu_word_t)) {
    _doILL(self);
    return;
  }

  const cpu_addr_t offset = SIGN_EXTEND(simm, 11, cpu_addr_t);
  const cpu_addr_t addr = _readReg(self, rs1) + offset;

  const cpu_word_t data = _readReg(self, rs2);
  _writeMem(self, addr, &data, size);
}

static inline void _doLOAD(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1) {
  const size_t size = 1 << (funct3 & 3U);
  if(size > sizeof(cpu_word_t)) {
    _doILL(self);
    return;
  }

  const bool isSigned = !(funct3 & 4U);

  const cpu_addr_t offset = SIGN_EXTEND(iimm, 11, cpu_addr_t);
  const cpu_addr_t addr = _readReg(self, rs1) + offset;

  cpu_word_t data = 0;
  _readMem(self, addr, &data, size);
  if(isSigned)
    data = SIGN_EXTEND(data, (size * 8) - 1, cpu_word_t);

  _writeReg(self, rd, data);
}

#ifdef CONFIG_RV64

static inline void _doOPIMM32(RV_Cpu* self, unsigned int funct3, uint32_t iimm, unsigned int rd, unsigned int rs1) {
  const cpu_word_t imm = SIGN_EXTEND(iimm, 11, cpu_word_t);
  const uint32_t rs1Val = _readReg(self, rs1);

  switch(funct3) {
    // ADDIW
    case 0: {
      const uint32_t res = imm + rs1Val;
      _writeReg(self, rd, SIGN_EXTEND(res, 31, cpu_word_t));
      break;
    }

    case 1: {
      switch(iimm >> 5) {
        case 0: {
          // SLLIW
          const uint32_t res = rs1Val << (iimm & 0x1F);
          _writeReg(self, rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 5: {
      const unsigned int shift = iimm & 0x1F;
      switch(iimm >> 5) {
        case 0: {
          // SRLIW
          const uint32_t res = rs1Val >> shift;
          _writeReg(self, rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        case 0x20: {
          // SRAIW
          const uint32_t sign = ~(((rs1Val & ((uint32_t)1 << ((sizeof(uint32_t) * 8) - 1))) >> shift) - 1);
          const uint32_t res = (rs1Val >> shift) | sign;
          _writeReg(self, rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

static inline void _doOP32(RV_Cpu* self, unsigned int funct3, unsigned int funct7, unsigned int rd, unsigned int rs1, unsigned int rs2) {
  const uint32_t rs1Val = _readReg(self, rs1);
  const uint32_t rs2Val = _readReg(self, rs2);

  switch(funct3) {
    case 0: {
      switch(funct7) {
        // ADDW
        case 0: _writeReg(self, rd, SIGN_EXTEND(rs1Val + rs2Val, 31, cpu_word_t)); break;

        // MULW
        case 1: _writeReg(self, rd, SIGN_EXTEND(rs1Val * rs2Val, 31, cpu_word_t)); break;

        // SUBW
        case 32: _writeReg(self, rd, SIGN_EXTEND(rs1Val - rs2Val, 31, cpu_word_t)); break;

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 1: {
      switch(funct7) {
        // SLLW
        case 0: _writeReg(self, rd, SIGN_EXTEND(rs1Val << (rs2Val & CPU_SHIFT_MASK), 31, cpu_word_t)); break;

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 4: {
      switch(funct7) {
        case 1: {
          // DIVW
          if(rs2Val) {
            if(rs1Val == INT32_MIN && rs2Val == UINT32_MAX)
              _writeReg(self, rd, SIGN_EXTEND(INT32_MIN, 31, cpu_word_t));
            else
              _writeReg(self, rd, SIGN_EXTEND((int32_t)rs1Val / (int32_t)rs2Val, 31, cpu_word_t));
          } else {
            _writeReg(self, rd, CPU_UINT_MAX);
          }
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 5: {
      switch(funct7) {
        // SRLW
        case 0: _writeReg(self, rd, SIGN_EXTEND(rs1Val >> (rs2Val & 0x1F), 31, cpu_word_t)); break;

        case 1: {
          // DIVUW
          if(rs2Val)
            _writeReg(self, rd, SIGN_EXTEND(rs1Val / rs2Val, 31, cpu_word_t));
          else
            _writeReg(self, rd, CPU_UINT_MAX);
          break;
        }

        case 32: {
          // SRAW
          const unsigned int shift = rs2Val & 0x1F;
          const uint32_t sign = ~(((rs1Val & ((uint32_t)1 << ((sizeof(uint32_t) * 8) - 1))) >> shift) - 1);
          const uint32_t res = (rs1Val >> shift) | sign;
          _writeReg(self, rd, SIGN_EXTEND(res, 31, cpu_word_t));
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 6: {
      switch(funct7) {
        case 1: {
          // REMW
          if(rs2Val) {
            if(rs1Val == INT32_MIN && rs2Val == UINT32_MAX)
              _writeReg(self, rd, 0);
            else
              _writeReg(self, rd, SIGN_EXTEND((int32_t)rs1Val % (int32_t)rs2Val, 31, cpu_word_t));
          } else {
            _writeReg(self, rd, SIGN_EXTEND(rs1Val, 31, cpu_word_t));
          }
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    case 7: {
      switch(funct7) {
        case 1: {
          // REMUW
          if(rs2Val)
            _writeReg(self, rd, SIGN_EXTEND(rs1Val % rs2Val, 31, cpu_word_t));
          else
            _writeReg(self, rd, SIGN_EXTEND(rs1Val, 31, cpu_word_t));
          break;
        }

        default: {
          _doILL(self);
          break;
        }
      }
      break;
    }

    default: {
      _doILL(self);
      break;
    }
  }
}

#endif

#endif //RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD
