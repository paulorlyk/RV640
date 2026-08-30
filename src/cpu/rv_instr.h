//
// Created by palulukan on 8/30/26.
//

#ifndef RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD
#define RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD

#include "rv64.h"

#include "rv_internal.h"
#include "rv_csr.h"

#include "../utils.h"

#include <assert.h>

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

    case 2: {
      // CBO
      switch(di->funct12) {
        default: {
          _trap(self, MCAUSE_INST_ILL, false);
          assert(false);
          break;
        }

        case 4: {
          // CBO.ZERO
          if(!di->rd) {
            static const uint8_t zero[DCACHE_LINE_SIZE] = {0};
            _writeMem(self, _readReg(self, di->rs1), zero, sizeof(zero));
          } else {
            _trap(self, MCAUSE_INST_ILL, false);
            assert(false);
          }
          break;
        }
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

static inline void _doILL(RV64_Cpu* self, const struct _instr *di) {
  (void)di;

  _trap(self, MCAUSE_INST_ILL, false);
  assert(false);
}

static inline void _execInstr(RV64_Cpu* self, uint32_t instr) {
  struct _instr di;
  di.size = 4;

  di.rd = (instr >> 7) & 0x1F;
  di.rs1 = (instr >> 15) & 0x1F;
  di.rs2 = (instr >> 20) & 0x1F;

  di.funct3 = (instr >> 12) & 0x7;
  di.funct5 = (instr >> 27) & 0x1F;
  di.funct7 = (instr >> 25) & 0x7F;
  di.funct12 = (instr >> 20) & 0xFFF;

  // di.aq = !!(instr & (1LU << 26));
  // di.rl = !!(instr & (1LU << 25));

  di.jimm = (instr & (0xFFLU << 12)) | ((instr >> 20) & (0x3FFLU << 1)) | ((instr >> 9) & (1LU << 11)) | ((instr >> 11) & (1LU << 20));
  di.iimm = (instr >> 20) & 0xFFF;
  di.bimm = ((instr >> 7) & (0xFU << 1)) | ((instr >> 20) & (0x3FU << 5)) | ((instr << 4) & (1U << 11)) | ((instr >> 19) & (1U << 12));
  di.uimm = instr & 0xFFFFF000LU;
  di.simm = ((instr >> 7) & 0x1F) | ((instr >> 20) & (0x7FU << 5));

  const unsigned int opcode = instr & 0x7F;
  switch(opcode) {
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
    default:   _doILL(self, &di); break;
  }
}

#endif //RV_INSTR_H_9AD35DA2EE06480DBD9A36468FBB44AD
