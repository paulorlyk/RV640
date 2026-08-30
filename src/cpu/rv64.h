//
// Created by palulukan on 7/24/26.
//

#ifndef RV64_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA
#define RV64_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA

#include "rv64_types.h"
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
  cpu_word_t PC;

  cpu_word_t mip; // Machine Interrupt-Pending
  cpu_word_t mie; // Machine Interrupt-Enable
  cpu_word_t mscratch; // Machine Scratch Register
  cpu_word_t mstatus; // Machine Status Register
  cpu_word_t mtvec; // Machine Trap-Vector Base-Address
  cpu_word_t mcause;  // Machine Cause
  cpu_word_t mepc;  // Machine Exception Program Counter Register
  cpu_word_t mtval; // Machine Trap Value Register
  cpu_word_t menvcfg; // Machine Environment Configuration Register

  bool trap;
  bool irq;

  Bus *bus;

  bool wfi;

  struct {
    cpu_addr_t base;
    uint8_t line[ICACHE_LINE_SIZE];
  } icache;

  RV64_PrivMode mode;

#ifdef CPU_STATS
  uint32_t icacheHits;
  uint32_t icacheMisses;
#endif
} RV64_Cpu;

bool rv64_init(RV64_Cpu* self, Bus *bus);
void rv64_destroy(RV64_Cpu *self);

#define rv64_getPC(self) ((self)->PC)
#define rv64_getRx(self, rx) ((self)->regs.Rx[(rx)])

#define rv64_isWFI(self) ((self)->wfi)
#define rv64_setRx(self, rx, val) ((self)->regs.Rx[(rx)] = (val))

void rv64_reset(RV64_Cpu *self, cpu_addr_t start);

void rv64_run(RV64_Cpu *self);

void rv64_setInterrupt(RV64_Cpu *self, RV64_MCAUSE n);
void rv64_clearInterrupt(RV64_Cpu *self, RV64_MCAUSE n);

#endif //RV64_H_2EAB5CD7ECCA42E6BF26D6DE898C51FA
