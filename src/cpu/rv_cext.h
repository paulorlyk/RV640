//
// Created by palulukan on 7/27/26.
//

#ifndef RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
#define RV_CEXT_H_77AE78DE749848389AC7122975A3F62A

#include "rv.h"

#include "rv_instr.h"

#include "../utils.h"

#include <stdbool.h>

static inline void _cextNop(RV_Cpu* self) {
  // nop -> addi x0, x0, 0
  // _doOPIMM(self, 0, 0, CPU_REG_ZERO, CPU_REG_ZERO);

  // No side effects, so no reason to execute a real instruction
  (void)self;
}

static inline void _decodeCextQ0_0(RV_Cpu* self, unsigned int instr) {
  const uint32_t imm = ((instr >> 2) & (1LU << 3)) | ((instr >> 4) & (1LU << 2)) | ((instr >> 1) & (0xFLU << 6)) | ((instr >> 7) & (0x3LU << 4));
  if(imm) {
    // c.addi4spn -> addi rd', x2, imm[9:2]
    const unsigned int rd = ((instr >> 2) & 0x7) + 8;

    _doOPIMM(self, 0, imm, rd, CPU_REG_SP);
  } else {
    // reserved -> illegal
    _doILL(self);
  }
}

static inline void _decodeCextQ0_1(RV_Cpu* self, unsigned int instr) {
  (void)instr;

  // ???
  _doILL(self);
}

static inline void _decodeCextQ0_2(RV_Cpu* self, unsigned int instr) {
  // c.lw -> lw rd', offset(rs1')
  const unsigned int rd = ((instr >> 2) & 0x7) + 8;
  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;
  const uint32_t iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));

  _doLOAD(self, 2, iimm, rd, rs1);
}

static inline void _decodeCextQ0_3(RV_Cpu* self, unsigned int instr) {
#ifdef CONFIG_RV64
  // c.ld -> ld rd', offset(rs1')
  const unsigned int rd = ((instr >> 2) & 0x7) + 8;
  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;
  const uint32_t iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));

  _doLOAD(self, 3, iimm, rd, rs1);
#else
  (void)instr;

  _doILL(self);
#endif
}

static inline void _decodeCextQ0_4(RV_Cpu* self, unsigned int instr) {
  (void)instr;

  // reserved -> illegal
  _doILL(self);
}

static inline void _decodeCextQ0_5(RV_Cpu* self, unsigned int instr) {
  (void)instr;

  // ???
  _doILL(self);
}

static inline void _decodeCextQ0_6(RV_Cpu* self, unsigned int instr) {
  // c.sw -> sw rs2', offset(rs1')
  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;
  const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
  const uint32_t simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));

  _doSTORE(self, 2, simm, rs1, rs2);
}

static inline void _decodeCextQ0_7(RV_Cpu* self, unsigned int instr) {
#ifdef CONFIG_RV64
  // c.sd -> sd rs2', offset(rs1')
  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;
  const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
  const unsigned int simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));

  _doSTORE(self, 3, simm, rs1, rs2);
#else
  (void)instr;

  _doILL(self);
#endif
}

static inline void _decodeCextQ1_0(RV_Cpu* self, unsigned int instr) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    const unsigned int imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
    if(imm) {
      // c.addi -> addi rd, rd, imm
      _doOPIMM(self, 0, SIGN_EXTEND(imm, 5, uint32_t), rd, rd);
    } else {
      // hint -> nop
      _cextNop(self);
    }
  } else {
    // imm != 0: hint -> nop
    // imm == 0: c.nop -> nop
    _cextNop(self);
  }
}

static inline void _decodeCextQ1_1(RV_Cpu* self, unsigned int instr) {
#ifdef CONFIG_RV64
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.addiw -> addiw rd, rd, imm
    const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

    _doOPIMM32(self, 0, SIGN_EXTEND(imm, 5, uint32_t), rd, rd);
  } else {
    // reserved -> illegal
    _doILL(self);
  }
#else
  // c.jal -> jal x1, offset
  const uint32_t imm = (instr >> 2) & 0x7FF;
  const uint32_t offset = SIGN_EXTEND(ASSEMBLE_11(imm, 5, 1, 2, 3, 7, 6, 10, 8, 9, 4, 11), 11, uint32_t);

  _doJAL(self, offset, CPU_REG_X1, 2);
#endif
}

static inline void _decodeCextQ1_2(RV_Cpu* self, unsigned int instr) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.li -> addi rd, x0, imm
    const unsigned int imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

    _doOPIMM(self, 0, SIGN_EXTEND(imm, 5, uint32_t), rd, CPU_REG_ZERO);
  } else {
    // hint -> nop
    _cextNop(self);
  }
}

static inline void _decodeCextQ1_3(RV_Cpu* self, unsigned int instr) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    if(rd == 2) {
      const uint32_t imm = ((instr >> 2) & (1LU << 4)) | ((instr << 3) & (1LU << 5)) | ((instr << 1) & (1LU << 6)) | ((instr << 4) & (3LU << 7)) | ((instr >> 3) & (1LU << 9));
      if(imm) {
        // c.addi16sp -> addi x2, x2, imm
        _doOPIMM(self, 0, SIGN_EXTEND(imm, 9, uint32_t), CPU_REG_SP, CPU_REG_SP);
      } else {
        // reserved -> illegal
        _doILL(self);
      }
    } else {
      const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
      if(imm) {
        // c.lui -> lui rd, imm
        _doLUI(self, SIGN_EXTEND(imm, 5, uint32_t) << 12, rd);
      } else {
        // reserved -> illegal
        _doILL(self);
      }
    }
  } else {
    // hint -> nop
    _cextNop(self);
  }
}

static inline void _decodeCextQ1_4(RV_Cpu* self, unsigned int instr) {
  const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
  const unsigned int rd = ((instr >> 7) & 0x7) + 8;
  switch((instr >> 10) & 3) {
    default:
    case 0: {
      // c.srli -> srli rd', rd', imm
      _doOPIMM(self, 5, imm, rd, rd);
      break;
    }

    case 1: {
      // c.srai -> srai rd', rd', shamt
      _doOPIMM(self, 5, imm | (1LU << 10), rd, rd);
      break;
    }

    case 2: {
      // c.andi -> andi rd', rd', imm
      _doOPIMM(self, 7, SIGN_EXTEND(imm, 5, uint32_t), rd, rd);
      break;
    }

    case 3: {
      if(instr & (1U << 12)) {
#ifdef CONFIG_RV64
        const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
        switch((instr >> 5) & 3) {
          default:
          case 0: {
            // c.subw -> subw rd', rd', rs2'
            _doOP32(self, 0, 32, rd, rd, rs2);
            break;
          }

          case 1: {
            // c.addw -> addw rd', rd', rs2'
            _doOP32(self, 0, 0, rd, rd, rs2);
            break;
          }

          case 2:
          case 3: {
            // reserved -> illegal
            _doILL(self);
            break;
          }
        }
#else
        _doILL(self);
#endif
      } else {
        const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
        switch((instr >> 5) & 3) {
          default:
          case 0: {
            // c.sub -> sub rd', rd', rs2'
            _doOP(self, 0, 32, rd, rd, rs2);
            break;
          }

          case 1: {
            // c.xor -> xor rd', rd', rs2'
            _doOP(self, 4, 0, rd, rd, rs2);
            break;
          }

          case 2: {
            // c.or -> or rd', rd', rs2'
            _doOP(self, 6, 0, rd, rd, rs2);
            break;
          }

          case 3: {
            // c.and -> and rd', rd', rs2'
            _doOP(self, 7, 0, rd, rd, rs2);
            break;
          }
        }
      }
      break;
    }
  }
}

static inline void _decodeCextQ1_5(RV_Cpu* self, unsigned int instr) {
  // c.j -> jal x0, offset
  const uint32_t imm = (instr >> 2) & 0x7FF;
  const uint32_t offset = SIGN_EXTEND(ASSEMBLE_11(imm, 5, 1, 2, 3, 7, 6, 10, 8, 9, 4, 11), 11, uint32_t);

  _doJAL(self, offset, CPU_REG_ZERO, 2);
}

static inline void _decodeCextQ1_6(RV_Cpu* self, unsigned int instr) {
  // c.beqz -> beq rs1', x0, offset
  const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
  const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;

  _doBRANCH(self, 0, offset, rs1, CPU_REG_ZERO, 2);
}

static inline void _decodeCextQ1_7(RV_Cpu* self, unsigned int instr) {
  // c.bnez -> bne rs1', x0, offset
  const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
  const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

  const unsigned int rs1 = ((instr >> 7) & 0x7) + 8;

  _doBRANCH(self, 1, offset, rs1, CPU_REG_ZERO, 2);
}

static inline void _decodeCextQ2_0(RV_Cpu* self, unsigned int instr) {
  const uint32_t shamt = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(shamt && rd) {
    // c.slli -> slli rd, rd, shamt
    _doOPIMM(self, 1, shamt, rd, rd);
  } else {
    // hint -> nop
    _cextNop(self);
  }
}

static inline void _decodeCextQ2_1(RV_Cpu* self, unsigned int instr) {
  (void)instr;

  // ???
  _doILL(self);
}

static inline void _decodeCextQ2_2(RV_Cpu* self, unsigned int instr) {
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.lwsp -> lw rd, offset(x2)
    const uint32_t iimm = ((instr << 4) & (3LU << 6)) | ((instr >> 2) & (7LU << 2)) | ((instr >> 7) & (1LU << 5));

    _doLOAD(self, 2, iimm, rd, CPU_REG_SP);
  } else {
    // reserved -> illegal
    _doILL(self);
  }
}

static inline void _decodeCextQ2_3(RV_Cpu* self, unsigned int instr) {
#ifdef CONFIG_RV64
  const unsigned int rd = (instr >> 7) & 0x1F;
  if(rd) {
    // c.ldsp -> ld rd, imm(x2)
    _doLOAD(self, 3, ((instr << 4) & (7LU << 6)) | ((instr >> 2) & (3LU << 3)) | ((instr >> 7) & (1LU << 5)), rd, CPU_REG_SP);
  } else {
    // reserved -> illegal
    _doILL(self);
  }
#else
  (void)instr;

  _doILL(self);
#endif
}

static inline void _decodeCextQ2_4(RV_Cpu* self, unsigned int instr) {
  const unsigned int uimm5 = (instr >> 12) & 1;
  const unsigned int rs1 = (instr >> 7) & 0x1F;
  const unsigned int rs2 = (instr >> 2) & 0x1F;
  if(!uimm5) {
    if(rs2) {
      if(rs1) {
        // c.mv -> add rd, x0, rs2
        _doOP(self, 0, 0, rs1, CPU_REG_ZERO, rs2);
      } else {
        // hint -> nop
        _cextNop(self);
      }
    } else {
      if(rs1) {
        // c.jr -> jalr x0, 0(rs1)
        _doJALR(self, 0, 0, CPU_REG_ZERO, rs1, 2);
      } else {
        // reserved -> illegal
        _doILL(self);
      }
    }
  } else {
    if(rs2) {
      if(rs1) {
        // c.add -> add rd, rd, rs2
        _doOP(self, 0, 0, rs1, rs1, rs2);
      } else {
        // hint -> nop
        _cextNop(self);
      }
    } else {
      if(rs1) {
        // c.jalr -> jalr x1, 0(rs1)
        _doJALR(self, 0, 0, CPU_REG_RA, rs1, 2);
      } else {
        // c.ebreak
        _doSYSTEM(self, 0, 1, CPU_REG_ZERO, CPU_REG_ZERO, 2);
      }
    }
  }
}

static inline void _decodeCextQ2_5(RV_Cpu* self, unsigned int instr) {
  (void)instr;

  // ???
  _doILL(self);
}

static inline void _decodeCextQ2_6(RV_Cpu* self, unsigned int instr) {
  // c.swsp -> sw rs2, imm(x2)
  const unsigned int rs2 = (instr >> 2) & 0x1F;
  const uint32_t simm = ((instr >> 7) & (0xFLU << 2)) | ((instr >> 1) & (3LU << 6));

  _doSTORE(self, 2, simm, CPU_REG_SP, rs2);
}

static inline void _decodeCextQ2_7(RV_Cpu* self, unsigned int instr) {
#ifdef CONFIG_RV64
  // c.sdsp -> sd rs2, imm(x2)
  _doSTORE(self, 3, ((instr >> 7) & (7LU << 3)) | ((instr >> 1) & (7LU << 6)), CPU_REG_SP, (instr >> 2) & 0x1F);
#else
  (void)instr;

  _doILL(self);
#endif
}

static inline bool _execCext(RV_Cpu* self, uint32_t instr) {
  typedef void (*decodeCextQx_y)(RV_Cpu* self, unsigned int instr);
  static const decodeCextQx_y jumpTable[24] = {
    _decodeCextQ0_0, _decodeCextQ0_1, _decodeCextQ0_2, _decodeCextQ0_3,
    _decodeCextQ0_4, _decodeCextQ0_5, _decodeCextQ0_6, _decodeCextQ0_7,

    _decodeCextQ1_0, _decodeCextQ1_1, _decodeCextQ1_2, _decodeCextQ1_3,
    _decodeCextQ1_4, _decodeCextQ1_5, _decodeCextQ1_6, _decodeCextQ1_7,

    _decodeCextQ2_0, _decodeCextQ2_1, _decodeCextQ2_2, _decodeCextQ2_3,
    _decodeCextQ2_4, _decodeCextQ2_5, _decodeCextQ2_6, _decodeCextQ2_7,
  };

  const unsigned int q = instr & 3U;
  if(q == 3)
    return false;

  const unsigned int funct3 = (instr >> 13) & 7;

  const unsigned int idx = (q << 3) | funct3;

  jumpTable[idx](self, instr & 0xFFFF);

  return true;
}

#endif //RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
