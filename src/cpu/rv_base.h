//
// Created by palulukan on 10/4/26.
//

#ifndef RV_BASE_H_69BB6C60A91945F0AC0D9777CA34E6B6
#define RV_BASE_H_69BB6C60A91945F0AC0D9777CA34E6B6

#include "rv_instr.h"

#define DECODE_RD(instr)      (((instr) >> 7) & 0x1F)
#define DECODE_RS1(instr)     (((instr) >> 15) & 0x1F)
#define DECODE_RS2(instr)     (((instr) >> 20) & 0x1F)
#define DECODE_FUNCT3(instr)  (((instr) >> 12) & 0x7)
#define DECODE_FUNCT5(instr)  (((instr) >> 27) & 0x1F)
#define DECODE_FUNCT7(instr)  (((instr) >> 25) & 0x7F)
#define DECODE_JIMM(instr)    (((instr) & (0xFFLU << 12)) | (((instr) >> 20) & (0x3FFLU << 1)) | (((instr) >> 9) & (1LU << 11)) | (((instr) >> 11) & (1LU << 20)))
#define DECODE_IIMM(instr)    (((instr) >> 20) & 0xFFF)
#define DECODE_BIMM(instr)    ((((instr) >> 7) & (0xFU << 1)) | (((instr) >> 20) & (0x3FU << 5)) | (((instr) << 4) & (1U << 11)) | (((instr) >> 19) & (1U << 12)))
#define DECODE_UIMM(instr)    ((instr) & 0xFFFFF000LU)
#define DECODE_SIMM(instr)    ((((instr) >> 7) & 0x1F) | (((instr) >> 20) & (0x7FU << 5)))

static inline void _decodeILL(RV_Cpu* self, uint32_t instr) {
  (void)instr;

  _doILL(self);
}

static inline void _decodeJAL(RV_Cpu* self, uint32_t instr) {
  _doJAL(self, DECODE_JIMM(instr), DECODE_RD(instr), 4);
}

static inline void _decodeSYSTEM(RV_Cpu* self, uint32_t instr) {
  _doSYSTEM(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr), 4);
}

static inline void _decodeMISCMEM(RV_Cpu* self, uint32_t instr) {
  _doMISCMEM(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr));
}

static inline void _decodeOPIMM(RV_Cpu* self, uint32_t instr) {
  _doOPIMM(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr));
}

static inline void _decodeBRANCH(RV_Cpu* self, uint32_t instr) {
  _doBRANCH(self, DECODE_FUNCT3(instr), DECODE_BIMM(instr), DECODE_RS1(instr), DECODE_RS2(instr), 4);
}

static inline void _decodeLUI(RV_Cpu* self, uint32_t instr) {
  _doLUI(self, DECODE_UIMM(instr), DECODE_RD(instr));
}

static inline void _decodeOP(RV_Cpu* self, uint32_t instr) {
  _doOP(self, DECODE_FUNCT3(instr), DECODE_FUNCT7(instr), DECODE_RD(instr), DECODE_RS1(instr), DECODE_RS2(instr));
}

static inline void _decodeJALR(RV_Cpu* self, uint32_t instr) {
  _doJALR(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr), 4);
}

static inline void _decodeAUIPC(RV_Cpu* self, uint32_t instr) {
  _doAUIPC(self, DECODE_UIMM(instr), DECODE_RD(instr));
}

static inline void _decodeAMO(RV_Cpu* self, uint32_t instr) {
  _doAMO(self, DECODE_FUNCT3(instr), DECODE_FUNCT5(instr), DECODE_RD(instr), DECODE_RS1(instr), DECODE_RS2(instr));
}

static inline void _decodeSTORE(RV_Cpu* self, uint32_t instr) {
  _doSTORE(self, DECODE_FUNCT3(instr), DECODE_SIMM(instr), DECODE_RS1(instr), DECODE_RS2(instr));
}

static inline void _decodeLOAD(RV_Cpu* self, uint32_t instr) {
  _doLOAD(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr));
}

static inline void _decodeOPIMM32(RV_Cpu* self, uint32_t instr) {
#ifdef CONFIG_RV64
  _doOPIMM32(self, DECODE_FUNCT3(instr), DECODE_IIMM(instr), DECODE_RD(instr), DECODE_RS1(instr));
#else
  _decodeILL(self, instr);
#endif
}

static inline void _decodeOP32(RV_Cpu* self, uint32_t instr) {
#ifdef CONFIG_RV64
  _doOP32(self, DECODE_FUNCT3(instr), DECODE_FUNCT7(instr), DECODE_RD(instr), DECODE_RS1(instr), DECODE_RS2(instr));
#else
  _decodeILL(self, instr);
#endif
}

static inline void _execInstr(RV_Cpu* self, uint32_t instr) {
  typedef void (*decodeBaseInstr)(RV_Cpu* self, uint32_t instr);
  static const decodeBaseInstr jumpTable[32] = {
    _decodeLOAD,    // 0
    _decodeILL,     // 1
    _decodeILL,     // 2
    _decodeMISCMEM, // 3
    _decodeOPIMM,   // 4
    _decodeAUIPC,   // 5
    _decodeOPIMM32, // 6
    _decodeILL,     // 7
    _decodeSTORE,   // 8
    _decodeILL,     // 9
    _decodeILL,     // 10
    _decodeAMO,     // 11
    _decodeOP,      // 12
    _decodeLUI,     // 13
    _decodeOP32,    // 14
    _decodeILL,     // 15
    _decodeILL,     // 16
    _decodeILL,     // 17
    _decodeILL,     // 18
    _decodeILL,     // 19
    _decodeILL,     // 20
    _decodeILL,     // 21
    _decodeILL,     // 22
    _decodeILL,     // 23
    _decodeBRANCH,  // 24
    _decodeJALR,    // 25
    _decodeILL,     // 26
    _decodeJAL,     // 27
    _decodeSYSTEM,  // 28
    _decodeILL,     // 29
    _decodeILL,     // 30
    _decodeILL,     // 31
  };

  const unsigned int opcode = ((unsigned int)instr >> 2) & 0x1F;
  jumpTable[opcode](self, instr);
}

#endif //RV_BASE_H_69BB6C60A91945F0AC0D9777CA34E6B6
