//
// Created by palulukan on 7/27/26.
//

#ifndef RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
#define RV_CEXT_H_77AE78DE749848389AC7122975A3F62A

#include "rv64.h"

#include "rv_instr.h"

#include "../utils.h"

#include <stdbool.h>
#include <assert.h>

static inline void _cextNop(RV64_Cpu* self, struct _instr *di) {
  di->funct3 = 0;
  di->rd = 0;
  di->rs1 = 0;
  di->iimm = 0;

  _doOPIMM(self, di);
}

static inline void _translateCextQ0_0(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const uint32_t imm = ((instr >> 2) & (1LU << 3)) | ((instr >> 4) & (1LU << 2)) | ((instr >> 1) & (1LU << 2)) | ((instr >> 1) & (0xFLU << 6)) | ((instr >> 7) & (0x3LU << 4));
  if(imm) {
    // c.addi4spn -> addi rd′, x2, imm[9:2]
    di->funct3 = 0;
    di->rd = ((instr >> 2) & 0x7) + 8;
    di->rs1 = CPU_REG_SP;
    di->iimm = imm;

    _doOPIMM(self, di);
  } else {
    // reserved -> illegal
    _doILL(self, di);
  }
}

static inline void _translateCextQ0_1(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  (void)instr;

  // ???
  _doILL(self, di);
}

static inline void _translateCextQ0_2(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.lw -> lw rd′, offset(rs1′)
  di->funct3 = 2;
  di->rd = ((instr >> 2) & 0x7) + 8;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));

  _doLOAD(self, di);
}

static inline void _translateCextQ0_3(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.ld -> ld rd′, offset(rs1′)
  di->funct3 = 3;
  di->rd = ((instr >> 2) & 0x7) + 8;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));

  _doLOAD(self, di);
}

static inline void _translateCextQ0_4(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  (void)instr;

  // reserved -> illegal
  _doILL(self, di);
}

static inline void _translateCextQ0_5(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  (void)instr;

  // ???
  _doILL(self, di);
}

static inline void _translateCextQ0_6(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.sw -> sw rs2′, offset(rs1′)
  di->funct3 = 2;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->rs2 = ((instr >> 2) & 0x7) + 8;
  di->simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));

  _doSTORE(self, di);
}

static inline void _translateCextQ0_7(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.sd -> sd rs2′, offset(rs1′)
  di->funct3 = 3;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->rs2 = ((instr >> 2) & 0x7) + 8;
  di->simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));

  _doSTORE(self, di);
}

static inline void _translateCextQ1_0(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    const unsigned int imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
    if(imm) {
      // c.addi -> addi rd, rd, imm
      di->funct3 = 0;
      di->rd = rd;
      di->rs1 = rd;
      di->iimm = SIGN_EXTEND(imm, 5, uint32_t);

      _doOPIMM(self, di);
    } else {
      // hint -> nop
      _cextNop(self, di);
    }
  } else {
    // imm != 0: hint -> nop
    // imm == 0: c.nop -> nop
    _cextNop(self, di);
  }
}

static inline void _translateCextQ1_1(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.addiw -> addiw rd, rd, imm
    const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

    di->funct3 = 0;
    di->rd = rd;
    di->rs1 = rd;
    di->iimm = SIGN_EXTEND(imm, 5, uint32_t);

    _doOPIMM32(self, di);
  } else {
    // reserved -> illegal
    _doILL(self, di);
  }
}

static inline void _translateCextQ1_2(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.li -> addi rd, x0, imm
    const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

    di->funct3 = 0;
    di->rd = rd;
    di->rs1 = CPU_REG_ZERO;
    di->iimm = SIGN_EXTEND(imm, 5, uint32_t);

    _doOPIMM(self, di);
  } else {
    // hint -> nop
    _cextNop(self, di);
  }
}

static inline void _translateCextQ1_3(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    if(rd == 2) {
      const uint32_t imm = ((instr >> 2) & (1LU << 4)) | ((instr << 3) & (1LU << 5)) | ((instr << 1) & (1LU << 6)) | ((instr << 4) & (3LU << 7)) | ((instr >> 3) & (1LU << 9));
      if(imm) {
        // c.addi16sp -> addi x2, x2, imm
        di->funct3 = 0;
        di->rd = CPU_REG_SP;
        di->rs1 = CPU_REG_SP;
        di->iimm = SIGN_EXTEND(imm, 9, uint32_t);

        _doOPIMM(self, di);
      } else {
        // reserved -> illegal
        _doILL(self, di);
      }
    } else {
      const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
      if(imm) {
        // c.lui -> lui rd, imm
        di->funct3 = 0;
        di->rd = rd;
        di->uimm = SIGN_EXTEND(imm, 5, uint32_t) << 12;

        _doLUI(self, di);
      } else {
        // reserved -> illegal
        _doILL(self, di);
      }
    }
  } else {
    // hint -> nop
    _cextNop(self, di);
  }
}

static inline void _translateCextQ1_4(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
  const unsigned int rd = ((instr >> 7) & 0x7) + 8;
  switch((instr >> 10) & 3) {
    default:
    case 0: {
      // c.srli -> srli rd′, rd′, imm
      di->funct3 = 5;
      di->rd = rd;
      di->rs1 = rd;
      di->iimm = imm;

      _doOPIMM(self, di);
      break;
    }

    case 1: {
      // c.srai -> srai rd′, rd′, shamt
      di->funct3 = 5;
      di->rd = rd;
      di->rs1 = rd;
      di->iimm = imm | (1LU << 10);

      _doOPIMM(self, di);
      break;
    }

    case 2: {
      // c.andi -> andi rd′, rd′, imm
      di->funct3 = 7;
      di->rd = rd;
      di->rs1 = rd;
      di->iimm = SIGN_EXTEND(imm, 5, uint32_t);

      _doOPIMM(self, di);
      break;
    }

    case 3: {
      if(instr & (1U << 12)) {
        const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
        switch((instr >> 5) & 3) {
          default:
          case 0: {
            // c.subw -> subw rd′, rd′, rs2′
            di->funct3 = 0;
            di->funct7 = 32;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP32(self, di);
            break;
          }

          case 1: {
            // c.addw -> addw rd′, rd′, rs2′
            di->funct3 = 0;
            di->funct7 = 0;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP32(self, di);
            break;
          }

          case 2:
          case 3: {
            // reserved -> illegal
            _doILL(self, di);
            break;
          }
        }
      } else {
        const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
        switch((instr >> 5) & 3) {
          default:
          case 0: {
            // c.sub -> sub rd′, rd′, rs2′
            di->funct3 = 0;
            di->funct7 = 32;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP(self, di);
            break;
          }

          case 1: {
            // c.xor -> xor rd′, rd′, rs2′
            di->funct3 = 4;
            di->funct7 = 0;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP(self, di);
            break;
          }

          case 2: {
            // c.or -> or rd′, rd′, rs2′
            di->funct3 = 6;
            di->funct7 = 0;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP(self, di);
            break;
          }

          case 3: {
            // c.and -> and rd′, rd′, rs2′
            di->funct3 = 7;
            di->funct7 = 0;
            di->rd = rd;
            di->rs1 = rd;
            di->rs2 = rs2;

            _doOP(self, di);
            break;
          }
        }
      }
      break;
    }
  }
}

static inline void _translateCextQ1_5(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.j -> jal x0, offset
  const uint32_t imm = (instr >> 2) & 0x7FF;
  const uint32_t offset = SIGN_EXTEND(ASSEMBLE_11(imm, 5, 1, 2, 3, 7, 6, 10, 8, 9, 4, 11), 11, uint32_t);

  di->rd = CPU_REG_ZERO;
  di->jimm = offset;

  _doJAL(self, di);
}

static inline void _translateCextQ1_6(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.beqz -> beq rs1', x0, offset
  const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
  const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

  di->funct3 = 0;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->rs2 = CPU_REG_ZERO;
  di->bimm = offset;

  _doBRANCH(self, di);
}

static inline void _translateCextQ1_7(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.bnez -> bne rs1', x0, offset
  const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
  const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

  di->funct3 = 1;
  di->rs1 = ((instr >> 7) & 0x7) + 8;
  di->rs2 = CPU_REG_ZERO;
  di->bimm = offset;

  _doBRANCH(self, di);
}

static inline void _translateCextQ2_0(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const uint32_t shamt = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(shamt && rd) {
    // c.slli -> slli rd, rd, shamt
    di->funct3 = 1;
    di->rd = rd;
    di->rs1 = rd;
    di->iimm = shamt;

    _doOPIMM(self, di);
  } else {
    // hint -> nop
    _cextNop(self, di);
  }
}

static inline void _translateCextQ2_1(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  (void)instr;

  // ???
  _doILL(self, di);
}

static inline void _translateCextQ2_2(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.lwsp -> lw rd, offset(x2)
    di->funct3 = 2;
    di->rd = rd;
    di->rs1 = CPU_REG_SP;
    di->iimm = ((instr << 4) & (3LU << 6)) | ((instr >> 2) & (7LU << 2)) | ((instr >> 7) & (1LU << 5));

    _doLOAD(self, di);
  } else {
    // reserved -> illegal
    _doILL(self, di);
  }
}

static inline void _translateCextQ2_3(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.ldsp -> ld rd, imm(x2)
    di->funct3 = 3;
    di->rd = rd;
    di->rs1 = CPU_REG_SP;
    di->iimm = ((instr << 4) & (7LU << 6)) | ((instr >> 2) & (3LU << 3)) | ((instr >> 7) & (1LU << 5));

    _doLOAD(self, di);
  } else {
    // reserved -> illegal
    _doILL(self, di);
  }
}

static inline void _translateCextQ2_4(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  const unsigned int uimm5 = (instr >> 12) & 1;
  const unsigned int rs1 = (instr >> 7) & 0x1F;
  const unsigned int rs2 = (instr >> 2) & 0x1F;
  if(!uimm5) {
    if(rs2) {
      if(rs1) {
        // c.mv -> add rd, x0, rs2
        di->funct3 = 0;
        di->funct7 = 0;
        di->rd = rs1;
        di->rs1 = CPU_REG_ZERO;
        di->rs2 = rs2;

        _doOP(self, di);
      } else {
        // hint -> nop
        _cextNop(self, di);
      }
    } else {
      if(rs1) {
        // c.jr -> jalr x0, 0(rs1)
        di->funct3 = 0;
        di->rd = CPU_REG_ZERO;
        di->rs1 = rs1;
        di->iimm = 0;

        _doJALR(self, di);
      } else {
        // reserved -> illegal
        _doILL(self, di);
      }
    }
  } else {
    if(rs2) {
      if(rs1) {
        // c.add -> add rd, rd, rs2
        di->funct3 = 0;
        di->funct7 = 0;
        di->rd = rs1;
        di->rs1 = rs1;
        di->rs2 = rs2;

        _doOP(self, di);
      } else {
        // hint -> nop
        _cextNop(self, di);
      }
    } else {
      if(rs1) {
        // c.jalr -> jalr x1, 0(rs1)
        di->funct3 = 0;
        di->rd = CPU_REG_RA;
        di->rs1 = rs1;
        di->iimm = 0;

        _doJALR(self, di);
      } else {
        // c.break
        assert(false);
      }
    }
  }
}

static inline void _translateCextQ2_5(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  (void)instr;

  // ???
  _doILL(self, di);
}

static inline void _translateCextQ2_6(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.swsp -> sw rs2, imm(x2)
  di->funct3 = 2;
  di->rs1 = CPU_REG_SP;
  di->rs2 = (instr >> 2) & 0x1F;
  di->simm = ((instr >> 7) & (0xFLU << 2)) | ((instr >> 1) & (3LU << 6));

  _doSTORE(self, di);
}

static inline void _translateCextQ2_7(RV64_Cpu* self, unsigned int instr, struct _instr *di) {
  // c.sdsp -> sd rs2, imm(x2)
  di->funct3 = 3;
  di->rs1 = CPU_REG_SP;
  di->rs2 = (instr >> 2) & 0x1F;
  di->simm = ((instr >> 7) & (7LU << 3)) | ((instr >> 1) & (7LU << 6));

  _doSTORE(self, di);
}

static inline bool _execCext(RV64_Cpu* self, uint32_t instr) {
  typedef void (*translateCextQx_y)(RV64_Cpu* self, unsigned int instr, struct _instr *di);
  static const translateCextQx_y jumpTable[24] = {
    _translateCextQ0_0, _translateCextQ0_1, _translateCextQ0_2, _translateCextQ0_3,
    _translateCextQ0_4, _translateCextQ0_5, _translateCextQ0_6, _translateCextQ0_7,

    _translateCextQ1_0, _translateCextQ1_1, _translateCextQ1_2, _translateCextQ1_3,
    _translateCextQ1_4, _translateCextQ1_5, _translateCextQ1_6, _translateCextQ1_7,

    _translateCextQ2_0, _translateCextQ2_1, _translateCextQ2_2, _translateCextQ2_3,
    _translateCextQ2_4, _translateCextQ2_5, _translateCextQ2_6, _translateCextQ2_7,
  };

  const unsigned int size = instr & 3U;
  if(size == 3)
    return false;

  const unsigned int funct3 = (instr >> 13) & 7;

  const unsigned int idx = size << 3 | funct3;

  struct _instr di;
  di.size = 2;

  jumpTable[idx](self, instr & 0xFFFF, &di);

  return true;
}

#endif //RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
