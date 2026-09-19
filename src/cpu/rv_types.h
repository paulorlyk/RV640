//
// Created by palulukan on 7/24/26.
//

#ifndef RV_TYPES_H_3A55870B9B434441AC8E30E67683E50E
#define RV_TYPES_H_3A55870B9B434441AC8E30E67683E50E

#include <stdint.h>
#include <inttypes.h>

#include "../utils.h"

typedef uint64_t cpu_word_t;
typedef int64_t  cpu_sword_t;

#ifdef CONFIG_RV64_32BIT_ADDR
typedef uint32_t cpu_addr_t;
typedef uint32_t cpu_size_t;
#else
typedef uint64_t cpu_addr_t;
typedef uint64_t cpu_size_t;
#endif

#define CPU_SIGN_BIT ((cpu_word_t)1 << ((sizeof(cpu_word_t) * 8) - 1))
#define CPU_UINT_MAX (~(cpu_word_t)0)
#define CPU_ADDR_MAX (~(cpu_addr_t)0)

#ifdef CONFIG_RV64_32BIT_ADDR
#define PRI_CPU_PTR PRIx32
#define PRI_CPU_SIZE PRIu32
#else
#define PRI_CPU_PTR PRIx64
#define PRI_CPU_SIZE PRIu64
#endif

#define PRI_CPU_XWORD PRIx64
#define PRI_CPU_UWORD PRIu64

typedef enum {
  CPU_REG_X0 = 0,
  CPU_REG_X1 = 1,
  CPU_REG_X2 = 2,
  CPU_REG_X3 = 3,
  CPU_REG_X4 = 4,
  CPU_REG_X5 = 5,
  CPU_REG_X6 = 6,
  CPU_REG_X7 = 7,
  CPU_REG_X8 = 8,
  CPU_REG_X9 = 9,
  CPU_REG_X10 = 10,
  CPU_REG_X11 = 11,
  CPU_REG_X12 = 12,
  CPU_REG_X13 = 13,
  CPU_REG_X14 = 14,
  CPU_REG_X15 = 15,
  CPU_REG_X16 = 16,
  CPU_REG_X17 = 17,
  CPU_REG_X18 = 18,
  CPU_REG_X19 = 19,
  CPU_REG_X20 = 20,
  CPU_REG_X21 = 21,
  CPU_REG_X22 = 22,
  CPU_REG_X23 = 23,
  CPU_REG_X24 = 24,
  CPU_REG_X25 = 25,
  CPU_REG_X26 = 26,
  CPU_REG_X27 = 27,
  CPU_REG_X28 = 28,
  CPU_REG_X29 = 29,
  CPU_REG_X30 = 30,
  CPU_REG_X31 = 31,

  // ABI synonyms
  CPU_REG_ZERO = CPU_REG_X0,  // hardwired to 0, ignores writes
  CPU_REG_RA = CPU_REG_X1,    // return address for jumps
  CPU_REG_SP = CPU_REG_X2,    // stack pointer
  CPU_REG_GP = CPU_REG_X3,    // global pointer
  CPU_REG_TP = CPU_REG_X4,    // thread pointer
  CPU_REG_T0 = CPU_REG_X5,    // temporary register 0
  CPU_REG_T1 = CPU_REG_X6,    // temporary register 1
  CPU_REG_T2 = CPU_REG_X7,    // temporary register 2
  CPU_REG_S0_FP = CPU_REG_X8, // saved register 0 or frame pointer
  CPU_REG_S1 = CPU_REG_X9,    // saved register 1
  CPU_REG_A0 = CPU_REG_X10,   // return value or function argument 0
  CPU_REG_A1 = CPU_REG_X11,   // return value or function argument 1
  CPU_REG_A2 = CPU_REG_X12,   // function argument 2
  CPU_REG_A3 = CPU_REG_X13,   // function argument 3
  CPU_REG_A4 = CPU_REG_X14,   // function argument 4
  CPU_REG_A5 = CPU_REG_X15,   // function argument 5
  CPU_REG_A6 = CPU_REG_X16,   // function argument 6
  CPU_REG_A7 = CPU_REG_X17,   // function argument 7
  CPU_REG_S2 = CPU_REG_X18,   // saved register 2
  CPU_REG_S3 = CPU_REG_X19,   // saved register 3
  CPU_REG_S4 = CPU_REG_X20,   // saved register 4
  CPU_REG_S5 = CPU_REG_X21,   // saved register 5
  CPU_REG_S6 = CPU_REG_X22,   // saved register 6
  CPU_REG_S7 = CPU_REG_X23,   // saved register 7
  CPU_REG_S8 = CPU_REG_X24,   // saved register 8
  CPU_REG_S9 = CPU_REG_X25,   // saved register 9
  CPU_REG_S10 = CPU_REG_X26,  // saved register 10
  CPU_REG_S11 = CPU_REG_X27,  // saved register 11
  CPU_REG_T3 = CPU_REG_X28,   // temporary register 3
  CPU_REG_T4 = CPU_REG_X29,   // temporary register 4
  CPU_REG_T5 = CPU_REG_X30,   // temporary register 5
  CPU_REG_T6 = CPU_REG_X31,   // temporary register 6
} RV_REG;

typedef enum {
  MCAUSE_SUPERVISOR_SW_INT = 1,   // Supervisor software interrupt
  MCAUSE_MACHINE_SW_INT = 3,      // Machine software interrupt
  MCAUSE_SUPERVISOR_TMR_INT = 5,  // Supervisor timer interrupt
  MCAUSE_MACHINE_TMR_INT = 7,     // Machine timer interrupt
  MCAUSE_SUPERVISOR_EXT_INT = 9,  // Supervisor external interrupt
  MCAUSE_MACHINE_EXT_INT = 11,    // Machine external interrupt
  MCAUSE_CTR_OVF_INT = 13,        // Counter-overflow interrupt

  MCAUSE_INST_ALLIGN = 0,         // Instruction address misaligned
  MCAUSE_INST_AF = 1,             // Instruction access fault
  MCAUSE_INST_ILL = 2,            // Illegal instruction
  MCAUSE_BREAKPOINT = 3,          // Breakpoint
  MCAUSE_LD_ALLIGN = 4,           // Load address misaligned
  MCAUSE_LD_AF = 5,               // Load access fault
  MCAUSE_ST_ALLIGN = 6,           // Store/AMO address misaligned
  MCAUSE_ST_AF = 7,               // Store/AMO access fault
  MCAUSE_CALL_U = 8,              // Environment call from U-mode
  MCAUSE_CALL_S = 9,              // Environment call from S-mode
  MCAUSE_CALL_M = 11,             // Environment call from M-mode
  MCAUSE_INST_PF = 12,            // Instruction page fault
  MCAUSE_LD_PF = 13,              // Load page fault
  MCAUSE_ST_PF = 15,              // Store/AMO page fault
  MCAUSE_DOUBLE_TRAP = 16,        // Double trap
  MCAUSE_SW_CHECK = 18,           // Software check
  MCAUSE_HW_ERR = 19,             // Hardware error
} RV_MCAUSE;

typedef enum {
  RV_PRIV_MODE_USER = 0,
  RV_PRIV_MODE_SUPERVISOR = 1,
  RV_PRIV_MODE_RESERVED = 2,
  RV_PRIV_MODE_MACHINE = 3,
} RV_PrivMode;

#define MIE_RW_MASK  ( \
    (1ULL << (unsigned int)MCAUSE_SUPERVISOR_SW_INT) \
  | (1ULL << (unsigned int)MCAUSE_MACHINE_SW_INT) \
  | (1ULL << (unsigned int)MCAUSE_SUPERVISOR_TMR_INT) \
  | (1ULL << (unsigned int)MCAUSE_MACHINE_TMR_INT) \
  | (1ULL << (unsigned int)MCAUSE_SUPERVISOR_EXT_INT) \
  | (1ULL << (unsigned int)MCAUSE_MACHINE_EXT_INT) \
  | (1ULL << (unsigned int)MCAUSE_CTR_OVF_INT) \
  | (~(cpu_word_t)0xFFFF) \
  )

#define MIP_RW_MASK  ((1ULL << (unsigned int)MCAUSE_SUPERVISOR_SW_INT) | (1ULL << (unsigned int)MCAUSE_SUPERVISOR_TMR_INT) | (1ULL << (unsigned int)MCAUSE_SUPERVISOR_EXT_INT))

#define MSTATUS_WPRI_MASK ((1ULL << 0) | (1ULL << 2) | (1ULL << 4) | (0x7FULL << 25) | (1ULL << 40) | (0x1FULL << 43) | (0x7FFFULL << 48))
#define MSTATUS_UBE_MASK  (1ULL << 6)                   // User-mode endianness
#define MSTATUS_SBE_MASK  (1ULL << 36)                  // Supervisor-mode endianness
#define MSTATUS_MBE_MASK  (1ULL << 37)                  // Machine-mode endianness

#define MSTATUS_UXL_MASK  ((1ULL << 32) | (1ULL << 33)) // XLEN for U-mode
#define MSTATUS_UXL_32    (1ULL << 32)
#define MSTATUS_UXL_64    (1ULL << 33)
#define MSTATUS_SXL_MASK  ((1ULL << 34) | (1ULL << 35)) // XLEN for S-mode
#define MSTATUS_SXL_32    (1ULL << 34)
#define MSTATUS_SXL_64    (1ULL << 35)

#define MSTATUS_VS_MASK   ((1ULL << 9) | (1ULL << 10))  // Vector extension state
#define MSTATUS_FS_MASK   ((1ULL << 13) | (1ULL << 14)) // Floating-point state
#define MSTATUS_XS_MASK   ((1ULL << 15) | (1ULL << 16)) // Additional extension state
#define MSTATUS_SD_MASK   (1ULL << 63)                  // State Dirty summary bit

#define MSTATUS_SIE_MASK  (1ULL << 1)                   // Supervisor Interrupt Enable
#define MSTATUS_MIE_MASK  (1ULL << 3)                   // Machine Interrupt Enable
#define MSTATUS_SPIE_MASK (1ULL << 5)                   // Supervisor Previous Interrupt Enable
#define MSTATUS_MPIE_MASK (1ULL << 7)                   // Machine Previous Interrupt Enable
#define MSTATUS_SPP_MASK  (1ULL << 8)                   // Previous supervisor privilege
#define MSTATUS_MPP_MASK  ((1ULL << 11) | (1ULL << 12)) // Previous machine privilege
#define MSTATUS_MPRV_MASK (1ULL << 17)                  // Modify PRiVilege
#define MSTATUS_SUM_MASK  (1ULL << 18)                  // Supervisor User Memory access
#define MSTATUS_MXR_MASK  (1ULL << 19)                  // Make eXecutable Readable
#define MSTATUS_TVM_MASK  (1ULL << 20)                  // Trap Virtual Memory
#define MSTATUS_TW_MASK   (1ULL << 21)                  // Timeout Wait
#define MSTATUS_TSR_MASK  (1ULL << 22)                  // Trap SRET

#define MSTATUS_WR_VAL(val) (((val) & ~(MSTATUS_WPRI_MASK | MSTATUS_UBE_MASK | MSTATUS_SBE_MASK | MSTATUS_MBE_MASK | MSTATUS_VS_MASK | MSTATUS_FS_MASK | MSTATUS_XS_MASK | MSTATUS_SD_MASK | MSTATUS_UXL_MASK | MSTATUS_SXL_MASK)) | MSTATUS_UXL_64 | MSTATUS_SXL_64)

DEFINE_MAX_FUNC(cpu_addr_t);
DEFINE_MIN_FUNC(cpu_addr_t);

DEFINE_MAX_FUNC(cpu_size_t);
DEFINE_MIN_FUNC(cpu_size_t);

#endif //RV_TYPES_H_3A55870B9B434441AC8E30E67683E50E
