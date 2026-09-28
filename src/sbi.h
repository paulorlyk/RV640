//
// Created by palulukan on 9/27/26.
//

#ifndef SBI_H_CA59CB6AFDEC4487B62BE252D982FA2A
#define SBI_H_CA59CB6AFDEC4487B62BE252D982FA2A

#include "cpu/rv.h"

typedef struct {
  cpu_sword_t error;
  union {
    cpu_sword_t value;
    cpu_word_t uvalue;
  };
} SBI_Ret;

typedef enum {
  SBI_SUCCESS = 0,                  // Completed successfully
  SBI_ERR_FAILED = -1,              // Failed
  SBI_ERR_NOT_SUPPORTED = -2,       // Not supported
  SBI_ERR_INVALID_PARAM = -3,       // Invalid parameter(s)
  SBI_ERR_DENIED = -4,              // Denied or not allowed
  SBI_ERR_INVALID_ADDRESS = -5,     // Invalid address(s)
  SBI_ERR_ALREADY_AVAILABLE = -6,   // Already available
  SBI_ERR_ALREADY_STARTED = -7,     // Already started
  SBI_ERR_ALREADY_STOPPED = -8,     // Already stopped
  SBI_ERR_NO_SHMEM = -9,            // Shared memory not available
  SBI_ERR_INVALID_STATE = -10,      // Invalid state
  SBI_ERR_BAD_RANGE = -11,          // Bad (or invalid) range
  SBI_ERR_TIMEOUT = -12,            // Failed due to timeout
  SBI_ERR_IO = -13,                 // Input/Output error
  SBI_ERR_DENIED_LOCKED = -14,      // Denied or not allowed due to lock status
} SBI_Error;

typedef enum {
  SBI_EID_BASE = 0x10,
  SBI_EID_TIMER = 0x54494D45,
  SBI_EID_IPI = 0x735049,
  SBI_EID_RFENCE = 0x52464E43,
  SBI_EID_SYSRESET = 0x53525354,
  SBI_EID_DBGCON = 0x4442434E,
  SBI_EID_FWFT = 0x46574654,
} SBI_EID;

void sbi_startHart(RV_Cpu* hart, cpu_addr_t entryPoint, cpu_word_t a1);

void sbi_handleEcall(RV_Cpu* hart);
void sbi_handleInterrupt(RV_Cpu* hart);
void sbi_handleException(RV_Cpu* hart);

#endif //SBI_H_CA59CB6AFDEC4487B62BE252D982FA2A
