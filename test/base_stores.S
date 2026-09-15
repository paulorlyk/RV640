/* =====================================================================
 * base_stores.S  --  RV64I base-ISA store instructions: SB, SH, SW, SD
 *
 * Part of the RV64I base-ISA suite; see base_tests.S for how this
 * plugs into the rest (main_tests.S -> base_tests.S -> here), and
 * common.S for the boot/UART/reporting harness everything sits on
 * ("check", "uart_puts" are used from there; nothing else).
 *
 * Stores are pure-write instructions: rs1 (base) and rs2 (value) are
 * both only ever read, never written back, so unlike the load suite
 * there's no rd sweep -- instead rs1 and rs2 each get their own
 * independent sweep across a representative sample of registers (same
 * "representative sample, not full 32" approach as base_loads.S, for
 * the same reason: a 5-bit register field can still have a wrong-bit
 * decode bug even though it isn't restricted the way RVC's compressed
 * formats are).
 *
 * There's no sign/zero-extension question for a store (nothing is
 * sign-extended on the way *out*), but there is a direct analog worth
 * just as much attention: SB/SH/SW must use only the low 8/16/32 bits
 * of rs2 and *discard* whatever is sitting in the upper bits, rather
 * than leaking them into memory. Each gets an explicit test for this
 * with a register deliberately loaded with distinctive garbage upper
 * bits. (SD has no such question -- all 64 bits are meaningful and
 * all 64 get written.)
 *
 * rs2=x0 (storing the always-zero register, a common real pattern) is
 * tested explicitly per instruction, not skipped. rs1=x0 is NOT
 * tested, same reasoning as the load suite: address 0 is unmapped in
 * this memory map and there's no access-fault handler, so it would
 * hang rather than usefully fail.
 *
 * Offsets are not constrained to natural alignment for the width being
 * stored -- see base_loads.S's header and the README for the same
 * caveat about misaligned-access portability to real hardware.
 * ===================================================================== */

    .option norvc            /* mandatory, not stylistic -- see base_loads.S */

    .global tests_base_stores

    .section .text

/* =======================================================================
 *  Shared macros, byte width (SB)
 * ======================================================================= */
.macro T_SB_RS2 rs2reg, name
    la      s1, store_anchor        /* s1, not s0: s0 is itself one of the
                                      * swept rs2reg candidates, and using
                                      * s0 here would destroy the address
                                      * just computed when rs2reg==s0 */
    li      \rs2reg, 0xaa
    sb      \rs2reg, 0(s1)
    lbu     t2, 0(s1)
    la      a0, \name
    li      a1, 0x00000000000000aa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SB_RS1 basereg, name
    la      \basereg, store_anchor
    li      a4, 0xaa
    sb      a4, 0(\basereg)
    lbu     t2, 0(\basereg)
    la      a0, \name
    li      a1, 0x00000000000000aa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SB_OFF off, name
    la      s0, store_anchor
    li      a4, 0xaa
    sb      a4, \off(s0)
    lbu     t2, \off(s0)
    la      a0, \name
    li      a1, 0x00000000000000aa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SB_VAL rawval, name
    la      s0, store_anchor
    li      a4, \rawval
    sb      a4, 0(s0)
    lbu     t2, 0(s0)
    la      a0, \name
    li      a1, (\rawval & 0xff)
    mv      a2, t2
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, halfword width (SH)
 * ======================================================================= */
.macro T_SH_RS2 rs2reg, name
    la      s1, store_anchor        /* s1, not s0 -- see T_SB_RS2 */
    li      \rs2reg, 0xaaaa
    sh      \rs2reg, 0(s1)
    lhu     t2, 0(s1)
    la      a0, \name
    li      a1, 0x000000000000aaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SH_RS1 basereg, name
    la      \basereg, store_anchor
    li      a4, 0xaaaa
    sh      a4, 0(\basereg)
    lhu     t2, 0(\basereg)
    la      a0, \name
    li      a1, 0x000000000000aaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SH_OFF off, name
    la      s0, store_anchor
    li      a4, 0xaaaa
    sh      a4, \off(s0)
    lhu     t2, \off(s0)
    la      a0, \name
    li      a1, 0x000000000000aaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SH_VAL rawval, name
    la      s0, store_anchor
    li      a4, \rawval
    sh      a4, 0(s0)
    lhu     t2, 0(s0)
    la      a0, \name
    li      a1, (\rawval & 0xffff)
    mv      a2, t2
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, word width (SW)
 * ======================================================================= */
.macro T_SW_RS2 rs2reg, name
    la      s1, store_anchor        /* s1, not s0 -- see T_SB_RS2 */
    li      \rs2reg, 0xaaaaaaaa
    sw      \rs2reg, 0(s1)
    lwu     t2, 0(s1)
    la      a0, \name
    li      a1, 0x00000000aaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SW_RS1 basereg, name
    la      \basereg, store_anchor
    li      a4, 0xaaaaaaaa
    sw      a4, 0(\basereg)
    lwu     t2, 0(\basereg)
    la      a0, \name
    li      a1, 0x00000000aaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SW_OFF off, name
    la      s0, store_anchor
    li      a4, 0xaaaaaaaa
    sw      a4, \off(s0)
    lwu     t2, \off(s0)
    la      a0, \name
    li      a1, 0x00000000aaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SW_VAL rawval, name
    la      s0, store_anchor
    li      a4, \rawval
    sw      a4, 0(s0)
    lwu     t2, 0(s0)
    la      a0, \name
    li      a1, (\rawval & 0xffffffff)
    mv      a2, t2
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, doubleword width (SD -- no upper-bits question)
 * ======================================================================= */
.macro T_SD_RS2 rs2reg, name
    la      s1, store_anchor        /* s1, not s0 -- see T_SB_RS2 */
    li      \rs2reg, 0xaaaaaaaaaaaaaaaa
    sd      \rs2reg, 0(s1)
    ld      t2, 0(s1)
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_RS1 basereg, name
    la      \basereg, store_anchor
    li      a4, 0xaaaaaaaaaaaaaaaa
    sd      a4, 0(\basereg)
    ld      t2, 0(\basereg)
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_OFF off, name
    la      s0, store_anchor
    li      a4, 0xaaaaaaaaaaaaaaaa
    sd      a4, \off(s0)
    ld      t2, \off(s0)
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_VAL rawval, name
    la      s0, store_anchor
    li      a4, \rawval
    sd      a4, 0(s0)
    ld      t2, 0(s0)
    la      a0, \name
    li      a1, \rawval
    mv      a2, t2
    jal     ra, check
.endm

/* =======================================================================
 *  SB : *(uint8_t *)(rs1 + off) = rs2[7:0]
 * ======================================================================= */
test_base_sb:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_SB_RS2 ra,  str_sb_rs2_ra
    T_SB_RS2 t0,  str_sb_rs2_t0
    T_SB_RS2 s0,  str_sb_rs2_s0
    T_SB_RS2 a0,  str_sb_rs2_a0
    T_SB_RS2 s10, str_sb_rs2_s10
    T_SB_RS2 t6,  str_sb_rs2_t6

    la      s0, store_anchor
    li      t0, 0x42
    sb      t0, 0(s0)
    sb      zero, 0(s0)
    lbu     t2, 0(s0)
    la      a0, str_sb_rs2_x0
    li      a1, 0
    mv      a2, t2
    jal     ra, check

    T_SB_RS1 ra,  str_sb_rs1_ra
    T_SB_RS1 t0,  str_sb_rs1_t0
    T_SB_RS1 s0,  str_sb_rs1_s0
    T_SB_RS1 a0,  str_sb_rs1_a0
    T_SB_RS1 s10, str_sb_rs1_s10
    T_SB_RS1 t6,  str_sb_rs1_t6

    T_SB_OFF 0,     str_sb_off0
    T_SB_OFF 4,     str_sb_off4
    T_SB_OFF -4,    str_sb_offm4
    T_SB_OFF 8,     str_sb_off8
    T_SB_OFF -8,    str_sb_offm8
    T_SB_OFF 2047,  str_sb_offmax
    T_SB_OFF -2048, str_sb_offmin

    T_SB_VAL 0x00, str_sb_val_zero
    T_SB_VAL 0x7f, str_sb_val_maxpos
    T_SB_VAL 0x80, str_sb_val_minneg
    T_SB_VAL 0xff, str_sb_val_allones
    T_SB_VAL 0x55, str_sb_val_alt0
    T_SB_VAL 0xaa, str_sb_val_alt1

    /* --- upper bits of rs2 (beyond bit 7) must be discarded --- */
    la      s0, store_anchor
    li      a4, 0xdeadbeefdeadbe42
    sb      a4, 0(s0)
    lbu     t2, 0(s0)
    la      a0, str_sb_upper_ignored
    li      a1, 0x42
    mv      a2, t2
    jal     ra, check

    /* --- invariants: rs1 and rs2 both unmodified (pure write) --- */
    la      s0, store_anchor
    li      a4, 0x77
    mv      t3, s0
    mv      t4, a4
    sb      a4, 0(s0)
    la      a0, str_sb_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check
    la      a0, str_sb_rs2_preserved
    mv      a1, t4
    mv      a2, a4
    jal     ra, check

    /* --- invariant: touches exactly 1 byte, nothing adjacent --- */
    la      s0, store_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, -8(s0)
    li      t0, 0xbbbbbbbbbbbbbbbb
    sd      t0, 8(s0)
    li      a4, 0x99
    sb      a4, 0(s0)
    ld      t1, -8(s0)
    la      a0, str_sb_adjacent_before
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t1
    jal     ra, check
    ld      t1, 8(s0)
    la      a0, str_sb_adjacent_after
    li      a1, 0xbbbbbbbbbbbbbbbb
    mv      a2, t1
    jal     ra, check
    lbu     t1, 0(s0)
    la      a0, str_sb_target_correct
    li      a1, 0x99
    mv      a2, t1
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  SH : *(uint16_t *)(rs1 + off) = rs2[15:0]
 * ======================================================================= */
test_base_sh:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_SH_RS2 ra,  str_sh_rs2_ra
    T_SH_RS2 t0,  str_sh_rs2_t0
    T_SH_RS2 s0,  str_sh_rs2_s0
    T_SH_RS2 a0,  str_sh_rs2_a0
    T_SH_RS2 s10, str_sh_rs2_s10
    T_SH_RS2 t6,  str_sh_rs2_t6

    la      s0, store_anchor
    li      t0, 0x4242
    sh      t0, 0(s0)
    sh      zero, 0(s0)
    lhu     t2, 0(s0)
    la      a0, str_sh_rs2_x0
    li      a1, 0
    mv      a2, t2
    jal     ra, check

    T_SH_RS1 ra,  str_sh_rs1_ra
    T_SH_RS1 t0,  str_sh_rs1_t0
    T_SH_RS1 s0,  str_sh_rs1_s0
    T_SH_RS1 a0,  str_sh_rs1_a0
    T_SH_RS1 s10, str_sh_rs1_s10
    T_SH_RS1 t6,  str_sh_rs1_t6

    T_SH_OFF 0,     str_sh_off0
    T_SH_OFF 4,     str_sh_off4
    T_SH_OFF -4,    str_sh_offm4
    T_SH_OFF 8,     str_sh_off8
    T_SH_OFF -8,    str_sh_offm8
    T_SH_OFF 2047,  str_sh_offmax
    T_SH_OFF -2048, str_sh_offmin

    T_SH_VAL 0x0000, str_sh_val_zero
    T_SH_VAL 0x7fff, str_sh_val_maxpos
    T_SH_VAL 0x8000, str_sh_val_minneg
    T_SH_VAL 0xffff, str_sh_val_allones
    T_SH_VAL 0x5555, str_sh_val_alt0
    T_SH_VAL 0xaaaa, str_sh_val_alt1

    /* --- upper bits of rs2 (beyond bit 15) must be discarded --- */
    la      s0, store_anchor
    li      a4, 0xdeadbeefcafe1234
    sh      a4, 0(s0)
    lhu     t2, 0(s0)
    la      a0, str_sh_upper_ignored
    li      a1, 0x1234
    mv      a2, t2
    jal     ra, check

    /* --- invariants: rs1 and rs2 both unmodified (pure write) --- */
    la      s0, store_anchor
    li      a4, 0x7777
    mv      t3, s0
    mv      t4, a4
    sh      a4, 0(s0)
    la      a0, str_sh_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check
    la      a0, str_sh_rs2_preserved
    mv      a1, t4
    mv      a2, a4
    jal     ra, check

    /* --- invariant: touches exactly 2 bytes, nothing adjacent --- */
    la      s0, store_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, -8(s0)
    li      t0, 0xbbbbbbbbbbbbbbbb
    sd      t0, 8(s0)
    li      a4, 0x9999
    sh      a4, 0(s0)
    ld      t1, -8(s0)
    la      a0, str_sh_adjacent_before
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t1
    jal     ra, check
    ld      t1, 8(s0)
    la      a0, str_sh_adjacent_after
    li      a1, 0xbbbbbbbbbbbbbbbb
    mv      a2, t1
    jal     ra, check
    lhu     t1, 0(s0)
    la      a0, str_sh_target_correct
    li      a1, 0x9999
    mv      a2, t1
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  SW : *(uint32_t *)(rs1 + off) = rs2[31:0]
 * ======================================================================= */
test_base_sw:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_SW_RS2 ra,  str_sw_rs2_ra
    T_SW_RS2 t0,  str_sw_rs2_t0
    T_SW_RS2 s0,  str_sw_rs2_s0
    T_SW_RS2 a0,  str_sw_rs2_a0
    T_SW_RS2 s10, str_sw_rs2_s10
    T_SW_RS2 t6,  str_sw_rs2_t6

    la      s0, store_anchor
    li      t0, 0x42424242
    sw      t0, 0(s0)
    sw      zero, 0(s0)
    lwu     t2, 0(s0)
    la      a0, str_sw_rs2_x0
    li      a1, 0
    mv      a2, t2
    jal     ra, check

    T_SW_RS1 ra,  str_sw_rs1_ra
    T_SW_RS1 t0,  str_sw_rs1_t0
    T_SW_RS1 s0,  str_sw_rs1_s0
    T_SW_RS1 a0,  str_sw_rs1_a0
    T_SW_RS1 s10, str_sw_rs1_s10
    T_SW_RS1 t6,  str_sw_rs1_t6

    T_SW_OFF 0,     str_sw_off0
    T_SW_OFF 4,     str_sw_off4
    T_SW_OFF -4,    str_sw_offm4
    T_SW_OFF 8,     str_sw_off8
    T_SW_OFF -8,    str_sw_offm8
    T_SW_OFF 2047,  str_sw_offmax
    T_SW_OFF -2048, str_sw_offmin

    T_SW_VAL 0x00000000, str_sw_val_zero
    T_SW_VAL 0x7fffffff, str_sw_val_maxpos
    T_SW_VAL 0x80000000, str_sw_val_minneg
    T_SW_VAL 0xffffffff, str_sw_val_allones
    T_SW_VAL 0x55555555, str_sw_val_alt0
    T_SW_VAL 0xaaaaaaaa, str_sw_val_alt1
    T_SW_VAL 0xdeadbeef, str_sw_val_deadbeef

    /* --- upper bits of rs2 (beyond bit 31) must be discarded --- */
    la      s0, store_anchor
    li      a4, 0xdeadbeef12345678
    sw      a4, 0(s0)
    lwu     t2, 0(s0)
    la      a0, str_sw_upper_ignored
    li      a1, 0x12345678
    mv      a2, t2
    jal     ra, check

    /* --- invariants: rs1 and rs2 both unmodified (pure write) --- */
    la      s0, store_anchor
    li      a4, 0x77777777
    mv      t3, s0
    mv      t4, a4
    sw      a4, 0(s0)
    la      a0, str_sw_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check
    la      a0, str_sw_rs2_preserved
    mv      a1, t4
    mv      a2, a4
    jal     ra, check

    /* --- invariant: touches exactly 4 bytes, nothing adjacent --- */
    la      s0, store_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, -8(s0)
    li      t0, 0xbbbbbbbbbbbbbbbb
    sd      t0, 8(s0)
    li      a4, 0x99999999
    sw      a4, 0(s0)
    ld      t1, -8(s0)
    la      a0, str_sw_adjacent_before
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t1
    jal     ra, check
    ld      t1, 8(s0)
    la      a0, str_sw_adjacent_after
    li      a1, 0xbbbbbbbbbbbbbbbb
    mv      a2, t1
    jal     ra, check
    lwu     t1, 0(s0)
    la      a0, str_sw_target_correct
    li      a1, 0x99999999
    mv      a2, t1
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  SD : *(uint64_t *)(rs1 + off) = rs2  -- no upper-bits question, all
 *       64 bits of rs2 are meaningful and all 64 get written.
 * ======================================================================= */
test_base_sd:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_SD_RS2 ra,  str_sd_rs2_ra
    T_SD_RS2 t0,  str_sd_rs2_t0
    T_SD_RS2 s0,  str_sd_rs2_s0
    T_SD_RS2 a0,  str_sd_rs2_a0
    T_SD_RS2 s10, str_sd_rs2_s10
    T_SD_RS2 t6,  str_sd_rs2_t6

    la      s0, store_anchor
    li      t0, 0x4242424242424242
    sd      t0, 0(s0)
    sd      zero, 0(s0)
    ld      t2, 0(s0)
    la      a0, str_sd_rs2_x0
    li      a1, 0
    mv      a2, t2
    jal     ra, check

    T_SD_RS1 ra,  str_sd_rs1_ra
    T_SD_RS1 t0,  str_sd_rs1_t0
    T_SD_RS1 s0,  str_sd_rs1_s0
    T_SD_RS1 a0,  str_sd_rs1_a0
    T_SD_RS1 s10, str_sd_rs1_s10
    T_SD_RS1 t6,  str_sd_rs1_t6

    T_SD_OFF 0,     str_sd_off0
    T_SD_OFF 4,     str_sd_off4
    T_SD_OFF -4,    str_sd_offm4
    T_SD_OFF 8,     str_sd_off8
    T_SD_OFF -8,    str_sd_offm8
    T_SD_OFF 2039,  str_sd_offmax    /* 2047 minus 8 bytes of store width */
    T_SD_OFF -2048, str_sd_offmin

    T_SD_VAL 0x0000000000000000, str_sd_val_zero
    T_SD_VAL 0x7fffffffffffffff, str_sd_val_maxpos
    T_SD_VAL 0x8000000000000000, str_sd_val_minneg
    T_SD_VAL 0xffffffffffffffff, str_sd_val_allones
    T_SD_VAL 0x5555555555555555, str_sd_val_alt0
    T_SD_VAL 0xaaaaaaaaaaaaaaaa, str_sd_val_alt1
    T_SD_VAL 0x0123456789abcdef, str_sd_val_walking
    T_SD_VAL 0x0000000000000001, str_sd_val_one

    /* --- invariants: rs1 and rs2 both unmodified (pure write) --- */
    la      s0, store_anchor
    li      a4, 0x7777777777777777
    mv      t3, s0
    mv      t4, a4
    sd      a4, 0(s0)
    la      a0, str_sd_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check
    la      a0, str_sd_rs2_preserved
    mv      a1, t4
    mv      a2, a4
    jal     ra, check

    /* --- invariant: touches exactly 8 bytes, nothing adjacent --- */
    la      s0, store_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, -8(s0)
    li      t0, 0xbbbbbbbbbbbbbbbb
    sd      t0, 8(s0)
    li      a4, 0x9999999999999999
    sd      a4, 0(s0)
    ld      t1, -8(s0)
    la      a0, str_sd_adjacent_before
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t1
    jal     ra, check
    ld      t1, 8(s0)
    la      a0, str_sd_adjacent_after
    li      a1, 0xbbbbbbbbbbbbbbbb
    mv      a2, t1
    jal     ra, check
    ld      t1, 0(s0)
    la      a0, str_sd_target_correct
    li      a1, 0x9999999999999999
    mv      a2, t1
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  Entry point for this file
 * ======================================================================= */
tests_base_stores:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    jal     ra, test_base_sb
    jal     ra, test_base_sh
    jal     ra, test_base_sw
    jal     ra, test_base_sd

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  Strings
 * ======================================================================= */
    .section .rodata
str_sb_rs2_ra:  .asciz "SB rs2=ra(x1)"
str_sb_rs2_t0:  .asciz "SB rs2=t0(x5)"
str_sb_rs2_s0:  .asciz "SB rs2=s0(x8)"
str_sb_rs2_a0:  .asciz "SB rs2=a0(x10)"
str_sb_rs2_s10: .asciz "SB rs2=s10(x26)"
str_sb_rs2_t6:  .asciz "SB rs2=t6(x31)"
str_sb_rs2_x0:  .asciz "SB rs2=x0 stores zero"
str_sb_rs1_ra:  .asciz "SB rs1=ra(x1)"
str_sb_rs1_t0:  .asciz "SB rs1=t0(x5)"
str_sb_rs1_s0:  .asciz "SB rs1=s0(x8)"
str_sb_rs1_a0:  .asciz "SB rs1=a0(x10)"
str_sb_rs1_s10: .asciz "SB rs1=s10(x26)"
str_sb_rs1_t6:  .asciz "SB rs1=t6(x31)"
str_sb_off0:   .asciz "SB offset=0"
str_sb_off4:   .asciz "SB offset=+4"
str_sb_offm4:  .asciz "SB offset=-4"
str_sb_off8:   .asciz "SB offset=+8"
str_sb_offm8:  .asciz "SB offset=-8"
str_sb_offmax: .asciz "SB offset=+2047 (max)"
str_sb_offmin: .asciz "SB offset=-2048 (min)"
str_sb_val_zero:    .asciz "SB value=0x00"
str_sb_val_maxpos:  .asciz "SB value=0x7f"
str_sb_val_minneg:  .asciz "SB value=0x80"
str_sb_val_allones: .asciz "SB value=0xff"
str_sb_val_alt0:    .asciz "SB value=0x55"
str_sb_val_alt1:    .asciz "SB value=0xaa"
str_sb_upper_ignored:   .asciz "SB discards upper 56 bits of rs2"
str_sb_rs1_preserved:   .asciz "SB rs1 unmodified"
str_sb_rs2_preserved:   .asciz "SB rs2 unmodified"
str_sb_adjacent_before: .asciz "SB does not touch bytes before target"
str_sb_adjacent_after:  .asciz "SB does not touch bytes after target"
str_sb_target_correct:  .asciz "SB target byte correct"

str_sh_rs2_ra:  .asciz "SH rs2=ra(x1)"
str_sh_rs2_t0:  .asciz "SH rs2=t0(x5)"
str_sh_rs2_s0:  .asciz "SH rs2=s0(x8)"
str_sh_rs2_a0:  .asciz "SH rs2=a0(x10)"
str_sh_rs2_s10: .asciz "SH rs2=s10(x26)"
str_sh_rs2_t6:  .asciz "SH rs2=t6(x31)"
str_sh_rs2_x0:  .asciz "SH rs2=x0 stores zero"
str_sh_rs1_ra:  .asciz "SH rs1=ra(x1)"
str_sh_rs1_t0:  .asciz "SH rs1=t0(x5)"
str_sh_rs1_s0:  .asciz "SH rs1=s0(x8)"
str_sh_rs1_a0:  .asciz "SH rs1=a0(x10)"
str_sh_rs1_s10: .asciz "SH rs1=s10(x26)"
str_sh_rs1_t6:  .asciz "SH rs1=t6(x31)"
str_sh_off0:   .asciz "SH offset=0"
str_sh_off4:   .asciz "SH offset=+4"
str_sh_offm4:  .asciz "SH offset=-4"
str_sh_off8:   .asciz "SH offset=+8"
str_sh_offm8:  .asciz "SH offset=-8"
str_sh_offmax: .asciz "SH offset=+2047 (max)"
str_sh_offmin: .asciz "SH offset=-2048 (min)"
str_sh_val_zero:    .asciz "SH value=0x0000"
str_sh_val_maxpos:  .asciz "SH value=0x7fff"
str_sh_val_minneg:  .asciz "SH value=0x8000"
str_sh_val_allones: .asciz "SH value=0xffff"
str_sh_val_alt0:    .asciz "SH value=0x5555"
str_sh_val_alt1:    .asciz "SH value=0xaaaa"
str_sh_upper_ignored:   .asciz "SH discards upper 48 bits of rs2"
str_sh_rs1_preserved:   .asciz "SH rs1 unmodified"
str_sh_rs2_preserved:   .asciz "SH rs2 unmodified"
str_sh_adjacent_before: .asciz "SH does not touch bytes before target"
str_sh_adjacent_after:  .asciz "SH does not touch bytes after target"
str_sh_target_correct:  .asciz "SH target halfword correct"

str_sw_rs2_ra:  .asciz "SW rs2=ra(x1)"
str_sw_rs2_t0:  .asciz "SW rs2=t0(x5)"
str_sw_rs2_s0:  .asciz "SW rs2=s0(x8)"
str_sw_rs2_a0:  .asciz "SW rs2=a0(x10)"
str_sw_rs2_s10: .asciz "SW rs2=s10(x26)"
str_sw_rs2_t6:  .asciz "SW rs2=t6(x31)"
str_sw_rs2_x0:  .asciz "SW rs2=x0 stores zero"
str_sw_rs1_ra:  .asciz "SW rs1=ra(x1)"
str_sw_rs1_t0:  .asciz "SW rs1=t0(x5)"
str_sw_rs1_s0:  .asciz "SW rs1=s0(x8)"
str_sw_rs1_a0:  .asciz "SW rs1=a0(x10)"
str_sw_rs1_s10: .asciz "SW rs1=s10(x26)"
str_sw_rs1_t6:  .asciz "SW rs1=t6(x31)"
str_sw_off0:   .asciz "SW offset=0"
str_sw_off4:   .asciz "SW offset=+4"
str_sw_offm4:  .asciz "SW offset=-4"
str_sw_off8:   .asciz "SW offset=+8"
str_sw_offm8:  .asciz "SW offset=-8"
str_sw_offmax: .asciz "SW offset=+2047 (max)"
str_sw_offmin: .asciz "SW offset=-2048 (min)"
str_sw_val_zero:     .asciz "SW value=0x00000000"
str_sw_val_maxpos:   .asciz "SW value=0x7fffffff"
str_sw_val_minneg:   .asciz "SW value=0x80000000"
str_sw_val_allones:  .asciz "SW value=0xffffffff"
str_sw_val_alt0:     .asciz "SW value=0x55555555"
str_sw_val_alt1:     .asciz "SW value=0xaaaaaaaa"
str_sw_val_deadbeef: .asciz "SW value=0xdeadbeef"
str_sw_upper_ignored:   .asciz "SW discards upper 32 bits of rs2"
str_sw_rs1_preserved:   .asciz "SW rs1 unmodified"
str_sw_rs2_preserved:   .asciz "SW rs2 unmodified"
str_sw_adjacent_before: .asciz "SW does not touch bytes before target"
str_sw_adjacent_after:  .asciz "SW does not touch bytes after target"
str_sw_target_correct:  .asciz "SW target word correct"

str_sd_rs2_ra:  .asciz "SD rs2=ra(x1)"
str_sd_rs2_t0:  .asciz "SD rs2=t0(x5)"
str_sd_rs2_s0:  .asciz "SD rs2=s0(x8)"
str_sd_rs2_a0:  .asciz "SD rs2=a0(x10)"
str_sd_rs2_s10: .asciz "SD rs2=s10(x26)"
str_sd_rs2_t6:  .asciz "SD rs2=t6(x31)"
str_sd_rs2_x0:  .asciz "SD rs2=x0 stores zero"
str_sd_rs1_ra:  .asciz "SD rs1=ra(x1)"
str_sd_rs1_t0:  .asciz "SD rs1=t0(x5)"
str_sd_rs1_s0:  .asciz "SD rs1=s0(x8)"
str_sd_rs1_a0:  .asciz "SD rs1=a0(x10)"
str_sd_rs1_s10: .asciz "SD rs1=s10(x26)"
str_sd_rs1_t6:  .asciz "SD rs1=t6(x31)"
str_sd_off0:   .asciz "SD offset=0"
str_sd_off4:   .asciz "SD offset=+4"
str_sd_offm4:  .asciz "SD offset=-4"
str_sd_off8:   .asciz "SD offset=+8"
str_sd_offm8:  .asciz "SD offset=-8"
str_sd_offmax: .asciz "SD offset=+2039 (max minus store width)"
str_sd_offmin: .asciz "SD offset=-2048 (min)"
str_sd_val_zero:    .asciz "SD value=0x0000000000000000"
str_sd_val_maxpos:  .asciz "SD value=0x7fffffffffffffff"
str_sd_val_minneg:  .asciz "SD value=0x8000000000000000"
str_sd_val_allones: .asciz "SD value=0xffffffffffffffff"
str_sd_val_alt0:    .asciz "SD value=0x5555555555555555"
str_sd_val_alt1:    .asciz "SD value=0xaaaaaaaaaaaaaaaa"
str_sd_val_walking: .asciz "SD value=0x0123456789abcdef"
str_sd_val_one:      .asciz "SD value=0x0000000000000001"
str_sd_rs1_preserved:   .asciz "SD rs1 unmodified"
str_sd_rs2_preserved:   .asciz "SD rs2 unmodified"
str_sd_adjacent_before: .asciz "SD does not touch bytes before target"
str_sd_adjacent_after:  .asciz "SD does not touch bytes after target"
str_sd_target_correct:  .asciz "SD target doubleword correct"

/* =======================================================================
 *  Data buffer: store_anchor sits in the middle so both +2047 and
 *  -2048 (the full 12-bit signed I-type/S-type immediate range) stay
 *  inside the allocated region, with margin for the width of the
 *  store itself and the adjacent-memory sentinels (+-8 bytes).
 * ======================================================================= */
    .section .bss
    .align 3
    .space 2304
store_anchor:
    .space 2304
