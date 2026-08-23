//
// Created by palulukan on 7/24/26.
//

#ifndef UI_H_E33E09B534A44C759DC58A0B98093190
#define UI_H_E33E09B534A44C759DC58A0B98093190

#include "rv64.h"

typedef enum {
  UI_PS_IDLE,
  UI_PS_RD,
  UI_PS_WR,
} UIPageStatus;

void ui_init();
void ui_destroy();

void ui_rerender();

void ui_cps(uint32_t cps);
void ui_time(uint64_t ms);

void ui_page_status(UIPageStatus status);

char ui_getch();
void ui_putch(char c);

#endif //UI_H_E33E09B534A44C759DC58A0B98093190
