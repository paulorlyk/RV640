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

static uint16_t _statusLine[80] = {};

static uint16_t _pageStatus = ' ';
static bool _idle = false;

static void inline _printStatusLine() {
  uint16_t __far *videoMem = __libi86_MK_FP(0xB800, 0);

  _fmemcpy(videoMem, _statusLine, sizeof(_statusLine));
}

static void inline _pokeVideoMem(int offset, uint16_t data) {
  uint16_t __far *videoMem = __libi86_MK_FP(0xB800, 0);

  *(videoMem + offset) = data;
}

static inline void _renderPageStatus() {
  const uint16_t d = _idle ? ' ' : _pageStatus;

  _statusLine[0] = d;
  _pokeVideoMem(0, d);
}

#else

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static struct {
  struct termios tio;
} _ui;

#endif

void ui_init() {
#ifndef CONFIG_DOS
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
    _statusLine[80 - 8 + i] = 2U << 8U | (uint16_t)buf[i];

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
    _statusLine[i + 1] = 3U << 8U | (uint16_t)buf[i];

  _printStatusLine();
#endif
}

void ui_idle(bool bIdle) {
#ifdef CONFIG_DOS
  _idle = bIdle;

  _renderPageStatus();
#endif
}

void ui_page_status(UIPageStatus status) {
#ifdef CONFIG_DOS
  switch(status) {
    default:
    case UI_PS_IDLE:
      _pageStatus = 2U << 12U | (unsigned int)' ';
      break;

    case UI_PS_RD:
      _pageStatus = 1U << 12U | (unsigned int)' ';
      break;

    case UI_PS_WR:
      _pageStatus = 4U << 12U | (unsigned int)' ';
      break;
  }

  _renderPageStatus();
#endif
}

char ui_getch() {
#ifdef CONFIG_DOS
  if(!kbhit())
    return 0;

  return getch();

#else
  char c;
  if(read(STDERR_FILENO, &c, 1) != 1)
    return 0;

  return c;
#endif
}

void ui_putch(char c) {
  putc(c, stdout);
  fflush(stdout);

  ui_rerender();
}
