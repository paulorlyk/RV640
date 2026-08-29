//
// Created by palulukan on 8/9/26.
//

#ifndef PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF
#define PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF

#include "device.h"
#include "../rv64.h"

#define PLIC_HARTS 1

#define PLIC_INTERRUPTS 1

#define PLIC_CONTEXTS_PER_HART 2  // M + S modes
#define PLIC_CONTEXTS (PLIC_HARTS * PLIC_CONTEXTS_PER_HART)

#define PLIC_INTERRUPT_WORDS  (((PLIC_INTERRUPTS + 1) / (sizeof(uint32_t) * 8)) + (((PLIC_INTERRUPTS + 1) % (sizeof(uint32_t) * 8)) != 0))

typedef struct {
  Device dev;

  uint32_t priority[PLIC_INTERRUPTS];       // 0
  uint32_t pending[PLIC_INTERRUPT_WORDS];   // 0x1000
  struct {
    uint32_t enable[PLIC_INTERRUPT_WORDS];  // 0x2000   + context * 0x80
    uint32_t threshold;                     // 0x200000 + context * 0x1000
    uint32_t claim;                         // 0x200004 + context * 0x1000

    uint32_t pendingClaims[PLIC_INTERRUPT_WORDS];
  } contexts[PLIC_CONTEXTS];

  RV64_Cpu* harts[PLIC_HARTS];
} Plic;

bool plic_init(Plic* self, RV64_Cpu* harts[PLIC_HARTS]);

void plic_destroy(Plic *self);

#define plic_device(self) (&(self)->dev)

void plic_interrupt(Plic *self, unsigned int n);

#endif //PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF
