//
// Created by palulukan on 7/24/26.
//

#ifndef UI_H_E33E09B534A44C759DC58A0B98093190
#define UI_H_E33E09B534A44C759DC58A0B98093190

#include "cpu/rv.h"
#include "dev/ns16550a.h"

typedef enum {
  UI_PS_IDLE,
  UI_PS_RD,
  UI_PS_WR,
} UIPageStatus;

void ui_init(Ns16550a *uart);
void ui_destroy();

void ui_rerender();

void ui_cps(uint32_t cps);
void ui_time(uint64_t ms);
void ui_idle(bool bIdle);
void ui_page_status(UIPageStatus status);
void ui_swapRd(int kb);
void ui_swapWr(int kb);

bool ui_getch();
void ui_putch(char c);

#endif //UI_H_E33E09B534A44C759DC58A0B98093190
