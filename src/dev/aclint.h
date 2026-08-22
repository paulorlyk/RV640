//
// Created by palulukan on 8/9/26.
//

#ifndef ACLINT_H_1EB7E96A572247C5ABBBCD05B33CCB6C
#define ACLINT_H_1EB7E96A572247C5ABBBCD05B33CCB6C

#include "device.h"
#include "../rv64.h"

#define ACLINT_HARTS 1

typedef struct {
  Device dev;

  struct {
    uint32_t msip[ACLINT_HARTS];
  } mswi;

  struct {
    uint64_t mtimecmp[ACLINT_HARTS];
    uint64_t mtime;

    bool pindingIrq[ACLINT_HARTS];
  } mtimer;

  RV64_Cpu* harts[ACLINT_HARTS];
} Aclint;

bool aclint_init(Aclint* self, RV64_Cpu* harts[ACLINT_HARTS]);

void aclint_destroy(Aclint *self);

#define aclint_device(self) (&(self)->dev)

void aclint_tick(Aclint *self, unsigned int cycles);

#define aclint_mtime(self) ((self)->mtimer.mtime)

#endif //ACLINT_H_1EB7E96A572247C5ABBBCD05B33CCB6C
