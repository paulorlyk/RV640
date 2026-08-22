//
// Created by palulukan on 7/25/26.
//

#ifndef UTILS_H_EF6EF93B375142EE89A91BE89F1268C0
#define UTILS_H_EF6EF93B375142EE89A91BE89F1268C0

#define REPEAT_1(f)                 f(0)
#define REPEAT_2(f)   REPEAT_1(f)   f(1)
#define REPEAT_3(f)   REPEAT_2(f)   f(2)
#define REPEAT_4(f)   REPEAT_3(f)   f(3)
#define REPEAT_5(f)   REPEAT_4(f)   f(4)
#define REPEAT_7(f)   REPEAT_5(f)   f(5)
#define REPEAT_6(f)   REPEAT_6(f)   f(6)
#define REPEAT_8(f)   REPEAT_7(f)   f(7)
#define REPEAT_9(f)   REPEAT_8(f)   f(8)
#define REPEAT_10(f)  REPEAT_9(f)   f(9)
#define REPEAT_11(f)  REPEAT_10(f)  f(10)
#define REPEAT_12(f)  REPEAT_11(f)  f(11)
#define REPEAT_13(f)  REPEAT_12(f)  f(12)
#define REPEAT_14(f)  REPEAT_13(f)  f(13)
#define REPEAT_15(f)  REPEAT_14(f)  f(14)
#define REPEAT_16(f)  REPEAT_15(f)  f(15)
#define REPEAT_17(f)  REPEAT_16(f)  f(16)
#define REPEAT_18(f)  REPEAT_17(f)  f(17)
#define REPEAT_19(f)  REPEAT_18(f)  f(18)
#define REPEAT_20(f)  REPEAT_19(f)  f(19)
#define REPEAT_21(f)  REPEAT_20(f)  f(20)
#define REPEAT_22(f)  REPEAT_21(f)  f(21)
#define REPEAT_23(f)  REPEAT_22(f)  f(22)
#define REPEAT_24(f)  REPEAT_23(f)  f(23)
#define REPEAT_25(f)  REPEAT_24(f)  f(24)
#define REPEAT_26(f)  REPEAT_25(f)  f(25)
#define REPEAT_27(f)  REPEAT_26(f)  f(26)
#define REPEAT_28(f)  REPEAT_27(f)  f(27)
#define REPEAT_29(f)  REPEAT_28(f)  f(28)
#define REPEAT_30(f)  REPEAT_29(f)  f(29)
#define REPEAT_31(f)  REPEAT_30(f)  f(30)
#define REPEAT_32(f)  REPEAT_31(f)  f(31)

#define REPEAT_(f, n) REPEAT_##n(f)
#define REPEAT(f, n)  REPEAT_(f, n)

#define ARR_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ALIGN_DOWN(v, alignment) ((v) & ~((alignment) - 1LU))
#define ALIGN_UP(v, alignment) ALIGN_DOWN((v) + (alignment) - 1LU, alignment)

#define SIGN_EXTEND(v, signBit, type) (((type)(v)) | (((((type)(v)) >> (unsigned int)(signBit)) & 1U) * ~((((type)1U) << (unsigned int)(signBit)) - 1U)))

#define ASSEMBLE_11(data, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10) \
  ( \
      ((((data) >> 0U) & 1U) << (unsigned int)(r0)) \
    | ((((data) >> 1U) & 1U) << (unsigned int)(r1)) \
    | ((((data) >> 2U) & 1U) << (unsigned int)(r2)) \
    | ((((data) >> 3U) & 1U) << (unsigned int)(r3)) \
    | ((((data) >> 4U) & 1U) << (unsigned int)(r4)) \
    | ((((data) >> 5U) & 1U) << (unsigned int)(r5)) \
    | ((((data) >> 6U) & 1U) << (unsigned int)(r6)) \
    | ((((data) >> 7U) & 1U) << (unsigned int)(r7)) \
    | ((((data) >> 8U) & 1U) << (unsigned int)(r8)) \
    | ((((data) >> 9U) & 1U) << (unsigned int)(r9)) \
    | ((((data) >> 10U) & 1U) << (unsigned int)(r10)) \
  )

#define ASSEMBLE_12(data, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11) \
  ( \
    ASSEMBLE_11(data, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10) \
    | ((((data) >> 11U) & 1U) << (unsigned int)(r11)) \
  )

#define ASSEMBLE_20(data, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16, r17, r18, r19) \
  ( \
    ASSEMBLE_12(data, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11) \
    | ((((data) >> 12U) & 1U) << (unsigned int)(r12)) \
    | ((((data) >> 13U) & 1U) << (unsigned int)(r13)) \
    | ((((data) >> 14U) & 1U) << (unsigned int)(r14)) \
    | ((((data) >> 15U) & 1U) << (unsigned int)(r15)) \
    | ((((data) >> 16U) & 1U) << (unsigned int)(r16)) \
    | ((((data) >> 17U) & 1U) << (unsigned int)(r17)) \
    | ((((data) >> 18U) & 1U) << (unsigned int)(r18)) \
    | ((((data) >> 19U) & 1U) << (unsigned int)(r19)) \
  )

#endif //UTILS_H_EF6EF93B375142EE89A91BE89F1268C0
