//
// Created by palulukan on 7/27/26.
//

#include "rv_cext.h"

#include "log.h"
#include "utils.h"

#include <assert.h>

static inline void _cextNop(struct _instr *di) {
  di->opcode = 0x13;
  di->funct3 = 0;
  di->rd = 0;
  di->rs1 = 0;
  di->iimm = 0;
}

static inline void _cextIllegal(struct _instr *di) {
  di->opcode = 0;
}

static inline void _translateCextQ0(unsigned int instr, struct _instr *di) {
  const unsigned int funct3 = (instr >> 13) & 7;
  switch(funct3) {
    default:
    case 0: {
      const uint32_t imm = ((instr >> 2) & (1LU << 3)) | ((instr >> 4) & (1LU << 2)) | ((instr >> 1) & (1LU << 2)) | ((instr >> 1) & (0xFLU << 6)) | ((instr >> 7) & (0x3LU << 4));
      if(imm) {
        // c.addi4spn -> addi rd′, x2, imm[9:2]
        di->opcode = 0x13;
        di->funct3 = 0;
        di->rd = ((instr >> 2) & 0x7) + 8;
        di->rs1 = CPU_REG_SP;
        di->iimm = imm;
      } else {
        // reserved -> illegal
        _cextIllegal(di);
      }
      break;
    }

    case 1: {
      // ???
      _cextIllegal(di);
      assert(false);
      break;
    }

    case 2: {
      // c.lw -> lw rd′, offset(rs1′)
      di->opcode = 0x03;
      di->funct3 = 2;
      di->rd = ((instr >> 2) & 0x7) + 8;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));
      break;
    }

    case 3: {
      // c.ld -> ld rd′, offset(rs1′)
      di->opcode = 0x03;
      di->funct3 = 3;
      di->rd = ((instr >> 2) & 0x7) + 8;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->iimm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));
      break;
    }

    case 4: {
      // reserved -> illegal
      _cextIllegal(di);
      break;
    }

    case 5: {
      // ???
      _cextIllegal(di);
      assert(false);
      break;
    }

    case 6: {
      // c.sw -> sw rs2′, offset(rs1′)
      di->opcode = 0x23;
      di->funct3 = 2;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->rs2 = ((instr >> 2) & 0x7) + 8;
      di->simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (1LU << 6)) | ((instr >> 4) & (1LU << 2));
      break;
    }

    case 7: {
      // c.sd -> sd rs2′, offset(rs1′)
      di->opcode = 0x23;
      di->funct3 = 3;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->rs2 = ((instr >> 2) & 0x7) + 8;
      di->simm = ((instr >> 7) & (0x7LU << 3)) | ((instr << 1) & (3LU << 6));
      break;
    }
  }
}

static inline void _translateCextQ1(unsigned int instr, struct _instr *di) {
  const unsigned int funct3 = (instr >> 13) & 7;
  switch(funct3) {
    default:
    case 0: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        const unsigned int imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
        if(imm) {
          // c.addi -> addi rd, rd, imm
          di->opcode = 0x13;
          di->funct3 = 0;
          di->rd = rd;
          di->rs1 = rd;
          di->iimm = SIGN_EXTEND(imm, 5, uint32_t);
        } else {
          // hint -> nop
          _cextNop(di);
        }
      } else {
        // imm != 0: hint -> nop
        // imm == 0: c.nop -> nop
        _cextNop(di);
      }
      break;
    }

    case 1: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        // c.addiw -> addiw rd, rd, imm
        const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

        di->opcode = 0x1B;
        di->funct3 = 0;
        di->rd = rd;
        di->rs1 = rd;
        di->iimm = SIGN_EXTEND(imm, 5, uint32_t);
      } else {
        // reserved -> illegal
        _cextIllegal(di);
      }
      break;
    }

    case 2: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        // c.li -> addi rd, x0, imm
        const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));

        di->opcode = 0x13;
        di->funct3 = 0;
        di->rd = rd;
        di->rs1 = CPU_REG_ZERO;
        di->iimm = SIGN_EXTEND(imm, 5, uint32_t);
      } else {
        // hint -> nop
        _cextNop(di);
      }
      break;
    }

    case 3: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        if(rd == 2) {
          const uint32_t imm = ((instr >> 2) & (1LU << 4)) | ((instr << 3) & (1LU << 5)) | ((instr << 1) & (1LU << 6)) | ((instr << 4) & (3LU << 7)) | ((instr >> 3) & (1LU << 9));
          if(imm) {
            // c.addi16sp -> addi x2, x2, imm
            di->opcode = 0x13;
            di->funct3 = 0;
            di->rd = CPU_REG_SP;
            di->rs1 = CPU_REG_SP;
            di->iimm = SIGN_EXTEND(imm, 9, uint32_t);
          } else {
            // reserved -> illegal
            _cextIllegal(di);
          }
        } else {
          const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
          if(imm) {
            // c.lui -> lui rd, imm
            di->opcode = 0x37;
            di->funct3 = 0;
            di->rd = rd;
            di->uimm = SIGN_EXTEND(imm, 5, uint32_t) << 12;
          } else {
            // reserved -> illegal
            _cextIllegal(di);
          }
        }
      } else {
        // hint -> nop
        _cextNop(di);
      }
      break;
    }

    case 4: {
      const uint32_t imm = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
      const unsigned int rd = ((instr >> 7) & 0x7) + 8;
      switch((instr >> 10) & 3) {
        default:
        case 0: {
          // c.srli -> srli rd′, rd′, imm
          di->opcode = 0x13;
          di->funct3 = 5;
          di->rd = rd;
          di->rs1 = rd;
          di->iimm = imm;
          break;
        }

        case 1: {
          // c.srai -> srai rd′, rd′, shamt
          di->opcode = 0x13;
          di->funct3 = 5;
          di->rd = rd;
          di->rs1 = rd;
          di->iimm = imm | (1LU << 10);
          break;
        }

        case 2: {
          // c.andi -> andi rd′, rd′, imm
          di->opcode = 0x13;
          di->funct3 = 7;
          di->rd = rd;
          di->rs1 = rd;
          di->iimm = SIGN_EXTEND(imm, 5, uint32_t);
          break;
        }

        case 3: {
          if(instr & (1U << 12)) {
            const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
            switch((instr >> 5) & 3) {
              default:
              case 0: {
                // c.subw -> subw rd′, rd′, rs2′
                di->opcode = 0x3B;
                di->funct3 = 0;
                di->funct7 = 32;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }

              case 1: {
                // c.addw -> addw rd′, rd′, rs2′
                di->opcode = 0x3B;
                di->funct3 = 0;
                di->funct7 = 0;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }

              case 2:
              case 3: {
                // reserved -> illegal
                _cextIllegal(di);
                break;
              }
            }
          } else {
            const unsigned int rs2 = ((instr >> 2) & 0x7) + 8;
            switch((instr >> 5) & 3) {
              default:
              case 0: {
                // c.sub -> sub rd′, rd′, rs2′
                di->opcode = 0x33;
                di->funct3 = 0;
                di->funct7 = 32;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }

              case 1: {
                // c.xor -> xor rd′, rd′, rs2′
                di->opcode = 0x33;
                di->funct3 = 4;
                di->funct7 = 0;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }

              case 2: {
                // c.or -> or rd′, rd′, rs2′
                di->opcode = 0x33;
                di->funct3 = 6;
                di->funct7 = 0;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }

              case 3: {
                // c.and -> and rd′, rd′, rs2′
                di->opcode = 0x33;
                di->funct3 = 7;
                di->funct7 = 0;
                di->rd = rd;
                di->rs1 = rd;
                di->rs2 = rs2;
                break;
              }
            }
          }
          break;
        }
      }
      break;
    }

    case 5: {
      // c.j -> jal x0, offset
      const uint32_t imm = (instr >> 2) & 0x7FF;
      const uint32_t offset = SIGN_EXTEND(ASSEMBLE_11(imm, 5, 1, 2, 3, 7, 6, 10, 8, 9, 4, 11), 11, uint32_t);

      di->opcode = 0x6F;
      di->rd = CPU_REG_ZERO;
      di->jimm = offset;
      break;
    }

    case 6: {
      // c.beqz -> beq rs1', x0, offset
      const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
      const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

      di->opcode = 0x63;
      di->funct3 = 0;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->rs2 = CPU_REG_ZERO;
      di->bimm = offset;
      break;
    }

    case 7: {
      // c.bnez -> bne rs1', x0, offset
      const uint32_t imm = ((instr << 3) & (1LU << 5)) | ((instr >> 2) & (3LU << 1)) | ((instr << 1) & (3LU << 6)) | ((instr >> 7) & (3LU << 3)) | ((instr >> 4) & (1LU << 8));
      const uint32_t offset = SIGN_EXTEND(imm, 8, uint32_t);

      di->opcode = 0x63;
      di->funct3 = 1;
      di->rs1 = ((instr >> 7) & 0x7) + 8;
      di->rs2 = CPU_REG_ZERO;
      di->bimm = offset;
      break;
    }
  }
}

static inline void _translateCextQ2(unsigned int instr, struct _instr *di) {
  const unsigned int funct3 = (instr >> 13) & 7;
  switch(funct3) {
    default:
    case 0: {
      const uint32_t shamt = ((instr >> 2) & 0x1FLU) | ((instr >> 7) & (1LU << 5));
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(shamt && rd) {
        // c.slli -> slli rd, rd, shamt
        di->opcode = 0x13;
        di->funct3 = 1;
        di->rd = rd;
        di->rs1 = rd;
        di->iimm = shamt;
      } else {
        // hint -> nop
        _cextNop(di);
      }
      break;
    }

    case 1: {
      // ???
      _cextIllegal(di);
      assert(false);
      break;
    }

    case 2: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        // c.lwsp -> lw rd, offset(x2)
        di->opcode = 0x03;
        di->funct3 = 2;
        di->rd = rd;
        di->rs1 = CPU_REG_SP;
        di->iimm = ((instr << 4) & (3LU << 6)) | ((instr >> 2) & (7LU << 2)) | ((instr >> 7) & (1LU << 5));
      } else {
        // reserved -> illegal
        _cextIllegal(di);
      }
      break;
    }

    case 3: {
      const unsigned int rd = (instr >> 7) & 0x1F;
      if(rd) {
        // c.ldsp -> ld rd, imm(x2)
        di->opcode = 0x03;
        di->funct3 = 3;
        di->rd = rd;
        di->rs1 = CPU_REG_SP;
        di->iimm = ((instr << 4) & (7LU << 6)) | ((instr >> 2) & (3LU << 3)) | ((instr >> 7) & (1LU << 5));
      } else {
        // reserved -> illegal
        _cextIllegal(di);
      }
      break;
    }

    case 4: {
      const unsigned int uimm5 = (instr >> 12) & 1;
      const unsigned int rs1 = (instr >> 7) & 0x1F;
      const unsigned int rs2 = (instr >> 2) & 0x1F;
      if(!uimm5) {
        if(rs2) {
          if(rs1) {
            // c.mv -> add rd, x0, rs2
            di->opcode = 0x33;
            di->funct3 = 0;
            di->funct7 = 0;
            di->rd = rs1;
            di->rs1 = CPU_REG_ZERO;
            di->rs2 = rs2;
          } else {
            // hint -> nop
            _cextNop(di);
          }
        } else {
          if(rs1) {
            // c.jr -> jalr x0, 0(rs1)
            di->opcode = 0x67;
            di->funct3 = 0;
            di->rd = CPU_REG_ZERO;
            di->rs1 = rs1;
            di->iimm = 0;
          } else {
            // reserved -> illegal
            _cextIllegal(di);
          }
        }
      } else {
        if(rs2) {
          if(rs1) {
            // c.add -> add rd, rd, rs2
            di->opcode = 0x33;
            di->funct3 = 0;
            di->funct7 = 0;
            di->rd = rs1;
            di->rs1 = rs1;
            di->rs2 = rs2;
          } else {
            // hint -> nop
            _cextNop(di);
          }
        } else {
          if(rs1) {
            // c.jalr -> jalr x1, 0(rs1)
            di->opcode = 0x67;
            di->funct3 = 0;
            di->rd = CPU_REG_RA;
            di->rs1 = rs1;
            di->iimm = 0;
          } else {
            // c.break
            assert(false);
          }
        }
      }
      break;
    }

    case 5: {
      // ???
      _cextIllegal(di);
      assert(false);
      break;
    }

    case 6: {
      // c.swsp -> sw rs2, imm(x2)
      di->opcode = 0x23;
      di->funct3 = 2;
      di->rs1 = CPU_REG_SP;
      di->rs2 = (instr >> 2) & 0x1F;
      di->simm = ((instr >> 7) & (0xFLU << 2)) | ((instr >> 1) & (3LU << 6));
      break;
    }

    case 7: {
      // c.sdsp -> sd rs2, imm(x2)
      di->opcode = 0x23;
      di->funct3 = 3;
      di->rs1 = CPU_REG_SP;
      di->rs2 = (instr >> 2) & 0x1F;
      di->simm = ((instr >> 7) & (7LU << 3)) | ((instr >> 1) & (7LU << 6));
      break;
    }
  }
}

bool rv_decodeCext(uint32_t instr, struct _instr *di) {
  switch(instr & 3U) {
    case 0: _translateCextQ0(instr & 0xFFFF, di); break;
    case 1: _translateCextQ1(instr & 0xFFFF, di); break;
    case 2: _translateCextQ2(instr & 0xFFFF, di); break;
    case 3:
    default:
      return false;
  }

  di->size = 2;
  di->instr = instr & 0xFFFF;

  return true;
}
