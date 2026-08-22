//
// Created by palulukan on 8/4/26.
//

#ifndef NS16550A_H_B08DFABE286A45C0B587E4484CF7181A
#define NS16550A_H_B08DFABE286A45C0B587E4484CF7181A

#include "device.h"

#define NS16550A_BUF_SIZE 256

typedef struct {
  Device dev;
  char buf[NS16550A_BUF_SIZE];
  int rdIdx;
  int wrIdx;
  int size;
} Ns16550a;

bool ns16550a_init(Ns16550a* self);

void ns16550a_destroy(Ns16550a *self);

#define ns16550_device(self) (&(self)->dev)

bool ns16550_push(Ns16550a *self, char ch);

#endif //NS16550A_H_B08DFABE286A45C0B587E4484CF7181A
