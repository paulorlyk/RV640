//
// Created by palulukan on 7/24/26.
//

#ifndef RV_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA
#define RV_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA

#include "rv_types.h"
#include "../bus.h"

#ifndef CONFIG_DOS
#include <endian.h>
#if __BYTE_ORDER != __LITTLE_ENDIAN
#error "This code assumes LE architecture"
#endif
#endif

// #define CPU_STATS

#define ICACHE_LINE_SIZE (128U)
#define DCACHE_LINE_SIZE (128U)

typedef struct {
  union {
    struct {
      cpu_word_t zero;
      cpu_word_t ra;
      cpu_word_t sp;
      cpu_word_t gp;
      cpu_word_t tp;
      cpu_word_t t0;
      cpu_word_t t1;
      cpu_word_t t2;
      cpu_word_t s0_fp;
      cpu_word_t s1;
      cpu_word_t a0;
      cpu_word_t a1;
      cpu_word_t a2;
      cpu_word_t a3;
      cpu_word_t a4;
      cpu_word_t a5;
      cpu_word_t a6;
      cpu_word_t a7;
      cpu_word_t s2;
      cpu_word_t s3;
      cpu_word_t s4;
      cpu_word_t s5;
      cpu_word_t s6;
      cpu_word_t s7;
      cpu_word_t s8;
      cpu_word_t s9;
      cpu_word_t s10;
      cpu_word_t s11;
      cpu_word_t t3;
      cpu_word_t t4;
      cpu_word_t t5;
      cpu_word_t t6;
    } abi;
    cpu_word_t Rx[32];
  } regs;
  cpu_addr_t PC;

  struct {
    cpu_word_t mip;       // Machine Interrupt-Pending
    cpu_word_t mie;       // Machine Interrupt-Enable
    cpu_word_t mscratch;  // Machine Scratch Register
    cpu_word_t mstatus;   // Machine Status Register
#ifndef CONFIG_RV64
    cpu_word_t mstatush;  // Additional Machine Status Register
#endif
    cpu_word_t mtvec;     // Machine Trap-Vector Base-Address
    cpu_word_t mcause;    // Machine Cause
    cpu_word_t mepc;      // Machine Exception Program Counter Register
    cpu_word_t mtval;     // Machine Trap Value Register
    cpu_word_t menvcfg;   // Machine Environment Configuration Register
  } csr;

  bool trap;
  bool irq;

  Bus *bus;

  bool wfi;

  struct {
    cpu_addr_t base;
    uint8_t line[ICACHE_LINE_SIZE];
  } icache;

  RV_PrivMode mode;

#ifdef CPU_STATS
  uint32_t icacheHits;
  uint32_t icacheMisses;
#endif
} RV_Cpu;

bool rv_init(RV_Cpu* self, Bus *bus);
void rv_destroy(RV_Cpu *self);

#define rv_getPC(self) ((self)->PC)
#define rv_getRx(self, rx) ((self)->regs.Rx[(rx)])

#define rv_isWFI(self) (!!((self)->wfi))
#define rv_setRx(self, rx, val) ((self)->regs.Rx[(rx)] = (val))

void rv_reset(RV_Cpu *self, cpu_addr_t start);

void rv_run(RV_Cpu *self);

void rv_setInterrupt(RV_Cpu *self, RV_MCAUSE n);
void rv_clearInterrupt(RV_Cpu *self, RV_MCAUSE n);

#endif //RV_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA
