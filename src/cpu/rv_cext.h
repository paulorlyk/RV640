//
// Created by palulukan on 7/27/26.
//

#ifndef RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
#define RV_CEXT_H_77AE78DE749848389AC7122975A3F62A

#include "rv64_types.h"

#include <stdbool.h>

bool rv_decodeCext(uint32_t instr, struct _instr *di);

#endif //RV_CEXT_H_77AE78DE749848389AC7122975A3F62A
