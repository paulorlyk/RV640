//
// Created by palulukan on 8/9/26.
//

#ifndef PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF
#define PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF

#include "device.h"

#define PLIC_HARTS 1

#define PLIC_CONTEXTS PLIC_HARTS

#define PLIC_INTERRUPTS 8
#define PLIC_INTERRUPT_WORDS  (PLIC_INTERRUPTS / (sizeof(uint32_t) * 8))

typedef struct {
  Device dev;

  uint32_t priority[PLIC_INTERRUPTS];       // 0
  uint32_t pending[PLIC_INTERRUPT_WORDS];   // 0x1000
  struct {
    uint32_t enable[PLIC_INTERRUPT_WORDS];  // 0x2000   + context * 0x80
    uint32_t threshold;                     // 0x200000 + context * 0x1000
    uint32_t calimComplete;                 // 0x200004 + context * 0x1000
  } contexts[PLIC_CONTEXTS];
} Plic;

bool plic_init(Plic* self);

void plic_destroy(Plic *self);

#define plic_device(self) (&(self)->dev)

void plic_int(Plic *self, int n);

#endif //PLIC_H_E5E4A5FAA02944E8B4BA0095454426CF
