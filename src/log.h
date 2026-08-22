//
// Created by palulukan on 7/24/26.
//

#ifndef LOG_H_506893879D394396B5438D08C17787BC
#define LOG_H_506893879D394396B5438D08C17787BC

#include "ui.h"

#include <stdio.h>

// NOLINTBEGIN
#define LOG(level, ...)             \
do {                                \
  fputs(level, stderr);             \
  fprintf(stderr, __VA_ARGS__);     \
  fputs("\n", stderr);              \
  ui_rerender();                    \
} while(false)                      \
// NOLINTEND

#define DEBUG(...)  LOG("[DEBUG]: ", __VA_ARGS__)

#define INFO(...)   LOG("[INFO ]: ", __VA_ARGS__)

#define WARN(...)   LOG("[WARN ]: ", __VA_ARGS__)

#define ERROR(...)  LOG("[ERROR]: ", __VA_ARGS__)

#endif //LOG_H_506893879D394396B5438D08C17787BC
