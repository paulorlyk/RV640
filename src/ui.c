//
// Created by palulukan on 7/24/26.
//

#include "ui.h"

#ifdef CONFIG_DOS

#include <i86.h>
#include <string.h>
#include <stddef.h>
#include <conio.h>
#include <stdio.h>
#include <ctype.h>
#include <dos.h>

static struct {
  uint16_t statusLine[80];

  uint16_t pageStatus;
  bool idle;

  uint32_t swapRdKb;
  uint32_t swapWrKb;

  Ns16550a *uart;
} _ui;

static inline void _printStatusLine() {
  uint16_t __far *videoMem = __libi86_MK_FP(0xB800, 0);

  _fmemcpy(videoMem, _ui.statusLine, sizeof(_ui.statusLine));
}

static inline void _pokeVideoMem(int offset, uint16_t data) {
  uint16_t __far *videoMem = __libi86_MK_FP(0xB800, 0);

  *(videoMem + offset) = data;
}

static inline void _renderPageStatus() {
  const uint16_t d = _ui.idle ? ' ' : _ui.pageStatus;

  _ui.statusLine[0] = d;
  _pokeVideoMem(0, d);
}

#else

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static struct {
  struct termios tio;

  Ns16550a *uart;
} _ui;

#endif

void ui_init(Ns16550a *uart) {
  _ui.uart = uart;

#ifdef CONFIG_DOS
  memset(_ui.statusLine, 0, sizeof(_ui.statusLine));

  _ui.pageStatus = ' ';
  _ui.idle = false;

  _ui.swapRdKb = 0;
  _ui.swapWrKb = 0;
#else
  tcgetattr(STDIN_FILENO, &_ui.tio);

  struct termios tio = _ui.tio;
  tio.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &tio);

  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
#endif
}

void ui_destroy() {
#ifndef CONFIG_DOS
  tcsetattr(STDIN_FILENO, TCSANOW, &_ui.tio);

  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

void ui_rerender() {
#ifdef CONFIG_DOS
  _printStatusLine();
#endif
}

void ui_cps(uint32_t cps) {
#ifdef CONFIG_DOS
  char buf[16] = {0};
  sprintf(buf, "%8" PRIu32, cps);

  for(int i = 0; buf[i]; ++i)
    _ui.statusLine[80 - 8 + i] = 2U << 8U | (uint16_t)buf[i];

  _printStatusLine();
#endif
}

void ui_time(uint64_t ms) {
#ifdef CONFIG_DOS
  uint64_t sec = ms / 1000ULL;
  uint64_t min = sec / 60;

  unsigned int h = min / 60;
  unsigned int m = min % 60;
  unsigned int s = sec % 60;

  char buf[16] = {0};
  sprintf(buf, "%5u:%02u:%02u", h, m, s);

  for(int i = 0; buf[i]; ++i)
    _ui.statusLine[i + 1] = 3U << 8U | (uint16_t)buf[i];

  _printStatusLine();
#endif
}

void ui_idle(bool bIdle) {
#ifdef CONFIG_DOS
  _ui.idle = bIdle;

  _renderPageStatus();
#endif
}

void ui_page_status(UIPageStatus status) {
#ifdef CONFIG_DOS
  switch(status) {
    default:
    case UI_PS_IDLE:
      _ui.pageStatus = 2U << 12U | (unsigned int)' ';
      break;

    case UI_PS_RD:
      _ui.pageStatus = 1U << 12U | (unsigned int)' ';
      break;

    case UI_PS_WR:
      _ui.pageStatus = 4U << 12U | (unsigned int)' ';
      break;
  }

  _renderPageStatus();
#endif
}

void ui_swapRd(int kb) {
#ifdef CONFIG_DOS
  _ui.swapRdKb += kb;

  char buf[16] = {0};
  sprintf(buf, "R: %8" PRIu32 "K", _ui.swapRdKb);

  for(int i = 0; buf[i]; ++i)
    _ui.statusLine[i + 14] = 9U << 8U | (uint16_t)buf[i];

  _printStatusLine();
#endif
}

void ui_swapWr(int kb) {
#ifdef CONFIG_DOS
  _ui.swapWrKb += kb;

  char buf[16] = {0};
  sprintf(buf, "W: %8" PRIu32 "K", _ui.swapWrKb);

  for(int i = 0; buf[i]; ++i)
    _ui.statusLine[i + 32] = 6U << 8U | (uint16_t)buf[i];

  _printStatusLine();
#endif
}

bool ui_getch() {
#ifdef CONFIG_DOS
  if(!kbhit())
    return true;

  char ch = getch();

  ns16550_push(_ui.uart, ch);
#else
  char ch;
  if(read(STDERR_FILENO, &ch, 1) != 1)
    return true;

  if(ch)
    ns16550_push(_ui.uart, ch);
#endif

  return true;
}

void ui_putch(char c) {
  putc(c, stdout);
  fflush(stdout);

  ui_rerender();
}
