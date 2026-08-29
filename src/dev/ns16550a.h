//
// Created by palulukan on 8/4/26.
//

#ifndef NS16550A_H_B08DFABE286A45C0B587E4484CF7181A
#define NS16550A_H_B08DFABE286A45C0B587E4484CF7181A

#include "device.h"
#include "plic.h"

#define NS16550A_BUF_SIZE 256

#define NS16550A_IRQ 1

#define NS16550A_LCR_DLAB (1U << 7)  // Divisor Latch Access Bit

#define NS16550A_LSR_TXE  (1U << 6)  // Transmitter Empty
#define NS16550A_LSR_THRE (1U << 5)  // THR Empty
#define NS16550A_LSR_DR   (1U << 0)  // Data Ready

#define NS16550A_IER_DR   (1U << 0)   // Data Ready
#define NS16550A_IER_THRE (1U << 1)   // THR Empty

#define NS16550A_ISR_IS         (1U << 0)   // Interrupt Status
#define NS16550A_ISR_IIC_OFFSET 1           // Interrupt Identification Code field offset
#define NS16550A_ISR_IIC_MASK   (7 << 1)    // Interrupt Identification Code field mask
#define NS16550A_ISR_IIC_NO     1           // Interrupt Identification Code - No interrupt
#define NS16550A_ISR_IIC_RLS    6           // Interrupt Identification Code - Receiver Line Status
#define NS16550A_ISR_IIC_RDR    4           // Interrupt Identification Code - Receiver Data Ready
#define NS16550A_ISR_IIC_RT     12          // Interrupt Identification Code - Reception Timeout
#define NS16550A_ISR_IIC_THRE   2           // Interrupt Identification Code - Transmitter Holding Register Empty
#define NS16550A_ISR_IIC_MS     0           // Interrupt Identification Code - Modem Status

typedef struct {
  Device dev;

  struct {
    char buf[NS16550A_BUF_SIZE];
    int rdIdx;
    int wrIdx;
    int len;
  } ringBuffer;

  uint8_t ier;      // Interrupt Enable Register
  uint8_t lcr;      // Line Control Register

  Plic *plic;
} Ns16550a;

bool ns16550a_init(Ns16550a* self, Plic* plic);

void ns16550a_destroy(Ns16550a *self);

#define ns16550_device(self) (&(self)->dev)

bool ns16550_push(Ns16550a *self, char ch);

#endif //NS16550A_H_B08DFABE286A45C0B587E4484CF7181A
