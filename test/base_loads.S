/* =====================================================================
 * base_loads.S  --  RV64I base-ISA load instructions: LB, LH, LW, LD,
 *                    LBU, LHU, LWU
 *
 * Part of the RV64I base-ISA suite; see base_tests.S for how this
 * plugs into the rest (main_tests.S -> base_tests.S -> here), and
 * common.S for the boot/UART/reporting harness everything sits on
 * ("check", "uart_puts" are used from there; nothing else).
 *
 * Unlike RVC, base-ISA loads have no restricted register fields (rd
 * and rs1 are each a full, independent 5-bit field) and no scrambled
 * immediate -- the 12-bit offset is a single contiguous field. That
 * changes where the real risk (and therefore the test coverage) is:
 *
 *   - No 3-bit-field decode risk, but a 5-bit register field can still
 *     have a wrong-bit bug, so each of rd and rs1 gets an independent
 *     sweep across a representative sample of registers (not
 *     exhaustively all 32 -- consistent with how full-5-bit RVC fields
 *     like C.ADDI/C.MV/C.JALR were sampled rather than swept in full).
 *   - No bit-scrambling to walk, so the offset only needs boundary
 *     values (+2047/-2048) and a couple of representative
 *     in-between ones, not an exhaustive per-bit sweep.
 *   - Sign vs. zero extension is where real bugs actually live for
 *     this instruction family (this project's own test-writing history
 *     includes more than one sign/zero-extension mixup), so each
 *     signed/unsigned pair (LB/LBU, LH/LHU, LW/LWU) gets the *same*
 *     underlying byte patterns run through both, so any accidental
 *     swap between the two is immediately visible as a mismatched
 *     expected value rather than something that could quietly pass.
 *
 * rd=x0 is tested once per instruction (confirms it doesn't fault and
 * genuinely discards the loaded value, rather than being skipped).
 * rs1=x0 is NOT tested: address 0 is unmapped in this memory map (RAM
 * starts at 0x80000000) and we don't have a load/store-access-fault
 * handler, so it would hang rather than usefully fail.
 *
 * Offsets here are not constrained to be naturally aligned for the
 * width being loaded -- the RISC-V base ISA permits (without
 * mandating hardware support for) misaligned accesses, and QEMU's TCG
 * emulation -- this project's primary validation target -- handles
 * them transparently. This is not guaranteed portable to all real
 * hardware; see the README.
 * ===================================================================== */

    .option norvc            /* mandatory here, not just stylistic: without
                               * it the assembler would silently substitute
                               * a compressed encoding whenever the operand
                               * choice happens to allow one, defeating the
                               * point of a suite that specifically tests
                               * the base (non-compressed) instructions. */

    .global tests_base_loads

    .section .text

/* =======================================================================
 *  Shared macros, byte width (LB / LBU)
 * ======================================================================= */
.macro T_LB8_RD insn, rdreg, expect, name
    la      s0, base_anchor
    li      t0, 0xaa
    sb      t0, 0(s0)
    \insn   \rdreg, 0(s0)
    mv      t2, \rdreg              /* capture before a0-a2 reused */
    la      a0, \name
    li      a1, \expect
    mv      a2, t2
    jal     ra, check
.endm

.macro T_LB8_RS1 insn, basereg, expect, name
    la      \basereg, base_anchor
    li      t1, 0xaa                /* t1, not t0: t0 is itself one of the
                                      * swept basereg candidates, and using
                                      * t0 here would destroy the address
                                      * just computed when basereg==t0 */
    sb      t1, 0(\basereg)
    \insn   a4, 0(\basereg)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LB8_OFF insn, off, expect, name
    la      s0, base_anchor
    li      t0, 0xaa
    sb      t0, \off(s0)
    \insn   a4, \off(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LB8_VAL insn, rawval, expect, name
    la      s0, base_anchor
    li      t0, \rawval
    sb      t0, 0(s0)
    \insn   a4, 0(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, halfword width (LH / LHU)
 * ======================================================================= */
.macro T_LH16_RD insn, rdreg, expect, name
    la      s0, base_anchor
    li      t0, 0xaaaa
    sh      t0, 0(s0)
    \insn   \rdreg, 0(s0)
    mv      t2, \rdreg
    la      a0, \name
    li      a1, \expect
    mv      a2, t2
    jal     ra, check
.endm

.macro T_LH16_RS1 insn, basereg, expect, name
    la      \basereg, base_anchor
    li      t1, 0xaaaa              /* t1, not t0 -- see T_LB8_RS1 */
    sh      t1, 0(\basereg)
    \insn   a4, 0(\basereg)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LH16_OFF insn, off, expect, name
    la      s0, base_anchor
    li      t0, 0xaaaa
    sh      t0, \off(s0)
    \insn   a4, \off(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LH16_VAL insn, rawval, expect, name
    la      s0, base_anchor
    li      t0, \rawval
    sh      t0, 0(s0)
    \insn   a4, 0(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, word width (LW / LWU)
 * ======================================================================= */
.macro T_LW32_RD insn, rdreg, expect, name
    la      s0, base_anchor
    li      t0, 0xaaaaaaaa
    sw      t0, 0(s0)
    \insn   \rdreg, 0(s0)
    mv      t2, \rdreg
    la      a0, \name
    li      a1, \expect
    mv      a2, t2
    jal     ra, check
.endm

.macro T_LW32_RS1 insn, basereg, expect, name
    la      \basereg, base_anchor
    li      t1, 0xaaaaaaaa          /* t1, not t0 -- see T_LB8_RS1 */
    sw      t1, 0(\basereg)
    \insn   a4, 0(\basereg)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LW32_OFF insn, off, expect, name
    la      s0, base_anchor
    li      t0, 0xaaaaaaaa
    sw      t0, \off(s0)
    \insn   a4, \off(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LW32_VAL insn, rawval, expect, name
    la      s0, base_anchor
    li      t0, \rawval
    sw      t0, 0(s0)
    \insn   a4, 0(s0)
    la      a0, \name
    li      a1, \expect
    mv      a2, a4
    jal     ra, check
.endm

/* =======================================================================
 *  Shared macros, doubleword width (LD only -- no signedness question)
 * ======================================================================= */
.macro T_LD64_RD rdreg, name
    la      s0, base_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, 0(s0)
    ld      \rdreg, 0(s0)
    mv      t2, \rdreg
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t2
    jal     ra, check
.endm

.macro T_LD64_RS1 basereg, name
    la      \basereg, base_anchor
    li      t1, 0xaaaaaaaaaaaaaaaa  /* t1, not t0 -- see T_LB8_RS1 */
    sd      t1, 0(\basereg)
    ld      a4, 0(\basereg)
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LD64_OFF off, name
    la      s0, base_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, \off(s0)
    ld      a4, \off(s0)
    la      a0, \name
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LD64_VAL rawval, name
    la      s0, base_anchor
    li      t0, \rawval
    sd      t0, 0(s0)
    ld      a4, 0(s0)
    la      a0, \name
    li      a1, \rawval
    mv      a2, a4
    jal     ra, check
.endm

/* =======================================================================
 *  LB : rd = sign_extend8(*(int8_t *)(rs1 + off))
 * ======================================================================= */
test_base_lb:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LB8_RD lb, ra,  0xffffffffffffffaa, str_lb_rd_ra
    T_LB8_RD lb, t0,  0xffffffffffffffaa, str_lb_rd_t0
    T_LB8_RD lb, s0,  0xffffffffffffffaa, str_lb_rd_s0
    T_LB8_RD lb, a0,  0xffffffffffffffaa, str_lb_rd_a0
    T_LB8_RD lb, s10, 0xffffffffffffffaa, str_lb_rd_s10
    T_LB8_RD lb, t6,  0xffffffffffffffaa, str_lb_rd_t6

    la      s0, base_anchor
    li      t0, 0xaa
    sb      t0, 0(s0)
    lb      zero, 0(s0)
    la      a0, str_lb_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LB8_RS1 lb, ra,  0xffffffffffffffaa, str_lb_rs1_ra
    T_LB8_RS1 lb, t0,  0xffffffffffffffaa, str_lb_rs1_t0
    T_LB8_RS1 lb, s0,  0xffffffffffffffaa, str_lb_rs1_s0
    T_LB8_RS1 lb, a0,  0xffffffffffffffaa, str_lb_rs1_a0
    T_LB8_RS1 lb, s10, 0xffffffffffffffaa, str_lb_rs1_s10
    T_LB8_RS1 lb, t6,  0xffffffffffffffaa, str_lb_rs1_t6

    T_LB8_OFF lb, 0,     0xffffffffffffffaa, str_lb_off0
    T_LB8_OFF lb, 4,     0xffffffffffffffaa, str_lb_off4
    T_LB8_OFF lb, -4,    0xffffffffffffffaa, str_lb_offm4
    T_LB8_OFF lb, 8,     0xffffffffffffffaa, str_lb_off8
    T_LB8_OFF lb, -8,    0xffffffffffffffaa, str_lb_offm8
    T_LB8_OFF lb, 2047,  0xffffffffffffffaa, str_lb_offmax
    T_LB8_OFF lb, -2048, 0xffffffffffffffaa, str_lb_offmin

    T_LB8_VAL lb, 0x00, 0x0000000000000000, str_lb_val_zero
    T_LB8_VAL lb, 0x7f, 0x000000000000007f, str_lb_val_maxpos
    T_LB8_VAL lb, 0x80, 0xffffffffffffff80, str_lb_val_minneg
    T_LB8_VAL lb, 0xff, 0xffffffffffffffff, str_lb_val_allones
    T_LB8_VAL lb, 0x55, 0x0000000000000055, str_lb_val_alt0
    T_LB8_VAL lb, 0xaa, 0xffffffffffffffaa, str_lb_val_alt1

    la      s0, base_anchor
    li      t0, 0x55
    sb      t0, 0(s0)
    mv      t3, s0
    lb      a4, 0(s0)
    la      a0, str_lb_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lbu     t4, 0(s0)
    la      a0, str_lb_mem_unchanged
    li      a1, 0x55
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LBU : rd = zero_extend8(*(uint8_t *)(rs1 + off))
 * ======================================================================= */
test_base_lbu:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LB8_RD lbu, ra,  0x00000000000000aa, str_lbu_rd_ra
    T_LB8_RD lbu, t0,  0x00000000000000aa, str_lbu_rd_t0
    T_LB8_RD lbu, s0,  0x00000000000000aa, str_lbu_rd_s0
    T_LB8_RD lbu, a0,  0x00000000000000aa, str_lbu_rd_a0
    T_LB8_RD lbu, s10, 0x00000000000000aa, str_lbu_rd_s10
    T_LB8_RD lbu, t6,  0x00000000000000aa, str_lbu_rd_t6

    la      s0, base_anchor
    li      t0, 0xaa
    sb      t0, 0(s0)
    lbu     zero, 0(s0)
    la      a0, str_lbu_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LB8_RS1 lbu, ra,  0x00000000000000aa, str_lbu_rs1_ra
    T_LB8_RS1 lbu, t0,  0x00000000000000aa, str_lbu_rs1_t0
    T_LB8_RS1 lbu, s0,  0x00000000000000aa, str_lbu_rs1_s0
    T_LB8_RS1 lbu, a0,  0x00000000000000aa, str_lbu_rs1_a0
    T_LB8_RS1 lbu, s10, 0x00000000000000aa, str_lbu_rs1_s10
    T_LB8_RS1 lbu, t6,  0x00000000000000aa, str_lbu_rs1_t6

    T_LB8_OFF lbu, 0,     0x00000000000000aa, str_lbu_off0
    T_LB8_OFF lbu, 4,     0x00000000000000aa, str_lbu_off4
    T_LB8_OFF lbu, -4,    0x00000000000000aa, str_lbu_offm4
    T_LB8_OFF lbu, 8,     0x00000000000000aa, str_lbu_off8
    T_LB8_OFF lbu, -8,    0x00000000000000aa, str_lbu_offm8
    T_LB8_OFF lbu, 2047,  0x00000000000000aa, str_lbu_offmax
    T_LB8_OFF lbu, -2048, 0x00000000000000aa, str_lbu_offmin

    T_LB8_VAL lbu, 0x00, 0x0000000000000000, str_lbu_val_zero
    T_LB8_VAL lbu, 0x7f, 0x000000000000007f, str_lbu_val_maxpos
    T_LB8_VAL lbu, 0x80, 0x0000000000000080, str_lbu_val_minneg
    T_LB8_VAL lbu, 0xff, 0x00000000000000ff, str_lbu_val_allones
    T_LB8_VAL lbu, 0x55, 0x0000000000000055, str_lbu_val_alt0
    T_LB8_VAL lbu, 0xaa, 0x00000000000000aa, str_lbu_val_alt1

    la      s0, base_anchor
    li      t0, 0x55
    sb      t0, 0(s0)
    mv      t3, s0
    lbu     a4, 0(s0)
    la      a0, str_lbu_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lbu     t4, 0(s0)
    la      a0, str_lbu_mem_unchanged
    li      a1, 0x55
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LH : rd = sign_extend16(*(int16_t *)(rs1 + off))
 * ======================================================================= */
test_base_lh:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LH16_RD lh, ra,  0xffffffffffffaaaa, str_lh_rd_ra
    T_LH16_RD lh, t0,  0xffffffffffffaaaa, str_lh_rd_t0
    T_LH16_RD lh, s0,  0xffffffffffffaaaa, str_lh_rd_s0
    T_LH16_RD lh, a0,  0xffffffffffffaaaa, str_lh_rd_a0
    T_LH16_RD lh, s10, 0xffffffffffffaaaa, str_lh_rd_s10
    T_LH16_RD lh, t6,  0xffffffffffffaaaa, str_lh_rd_t6

    la      s0, base_anchor
    li      t0, 0xaaaa
    sh      t0, 0(s0)
    lh      zero, 0(s0)
    la      a0, str_lh_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LH16_RS1 lh, ra,  0xffffffffffffaaaa, str_lh_rs1_ra
    T_LH16_RS1 lh, t0,  0xffffffffffffaaaa, str_lh_rs1_t0
    T_LH16_RS1 lh, s0,  0xffffffffffffaaaa, str_lh_rs1_s0
    T_LH16_RS1 lh, a0,  0xffffffffffffaaaa, str_lh_rs1_a0
    T_LH16_RS1 lh, s10, 0xffffffffffffaaaa, str_lh_rs1_s10
    T_LH16_RS1 lh, t6,  0xffffffffffffaaaa, str_lh_rs1_t6

    T_LH16_OFF lh, 0,     0xffffffffffffaaaa, str_lh_off0
    T_LH16_OFF lh, 4,     0xffffffffffffaaaa, str_lh_off4
    T_LH16_OFF lh, -4,    0xffffffffffffaaaa, str_lh_offm4
    T_LH16_OFF lh, 8,     0xffffffffffffaaaa, str_lh_off8
    T_LH16_OFF lh, -8,    0xffffffffffffaaaa, str_lh_offm8
    T_LH16_OFF lh, 2047,  0xffffffffffffaaaa, str_lh_offmax
    T_LH16_OFF lh, -2048, 0xffffffffffffaaaa, str_lh_offmin

    T_LH16_VAL lh, 0x0000, 0x0000000000000000, str_lh_val_zero
    T_LH16_VAL lh, 0x7fff, 0x0000000000007fff, str_lh_val_maxpos
    T_LH16_VAL lh, 0x8000, 0xffffffffffff8000, str_lh_val_minneg
    T_LH16_VAL lh, 0xffff, 0xffffffffffffffff, str_lh_val_allones
    T_LH16_VAL lh, 0x5555, 0x0000000000005555, str_lh_val_alt0
    T_LH16_VAL lh, 0xaaaa, 0xffffffffffffaaaa, str_lh_val_alt1

    la      s0, base_anchor
    li      t0, 0x5555
    sh      t0, 0(s0)
    mv      t3, s0
    lh      a4, 0(s0)
    la      a0, str_lh_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lhu     t4, 0(s0)
    la      a0, str_lh_mem_unchanged
    li      a1, 0x5555
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LHU : rd = zero_extend16(*(uint16_t *)(rs1 + off))
 * ======================================================================= */
test_base_lhu:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LH16_RD lhu, ra,  0x000000000000aaaa, str_lhu_rd_ra
    T_LH16_RD lhu, t0,  0x000000000000aaaa, str_lhu_rd_t0
    T_LH16_RD lhu, s0,  0x000000000000aaaa, str_lhu_rd_s0
    T_LH16_RD lhu, a0,  0x000000000000aaaa, str_lhu_rd_a0
    T_LH16_RD lhu, s10, 0x000000000000aaaa, str_lhu_rd_s10
    T_LH16_RD lhu, t6,  0x000000000000aaaa, str_lhu_rd_t6

    la      s0, base_anchor
    li      t0, 0xaaaa
    sh      t0, 0(s0)
    lhu     zero, 0(s0)
    la      a0, str_lhu_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LH16_RS1 lhu, ra,  0x000000000000aaaa, str_lhu_rs1_ra
    T_LH16_RS1 lhu, t0,  0x000000000000aaaa, str_lhu_rs1_t0
    T_LH16_RS1 lhu, s0,  0x000000000000aaaa, str_lhu_rs1_s0
    T_LH16_RS1 lhu, a0,  0x000000000000aaaa, str_lhu_rs1_a0
    T_LH16_RS1 lhu, s10, 0x000000000000aaaa, str_lhu_rs1_s10
    T_LH16_RS1 lhu, t6,  0x000000000000aaaa, str_lhu_rs1_t6

    T_LH16_OFF lhu, 0,     0x000000000000aaaa, str_lhu_off0
    T_LH16_OFF lhu, 4,     0x000000000000aaaa, str_lhu_off4
    T_LH16_OFF lhu, -4,    0x000000000000aaaa, str_lhu_offm4
    T_LH16_OFF lhu, 8,     0x000000000000aaaa, str_lhu_off8
    T_LH16_OFF lhu, -8,    0x000000000000aaaa, str_lhu_offm8
    T_LH16_OFF lhu, 2047,  0x000000000000aaaa, str_lhu_offmax
    T_LH16_OFF lhu, -2048, 0x000000000000aaaa, str_lhu_offmin

    T_LH16_VAL lhu, 0x0000, 0x0000000000000000, str_lhu_val_zero
    T_LH16_VAL lhu, 0x7fff, 0x0000000000007fff, str_lhu_val_maxpos
    T_LH16_VAL lhu, 0x8000, 0x0000000000008000, str_lhu_val_minneg
    T_LH16_VAL lhu, 0xffff, 0x000000000000ffff, str_lhu_val_allones
    T_LH16_VAL lhu, 0x5555, 0x0000000000005555, str_lhu_val_alt0
    T_LH16_VAL lhu, 0xaaaa, 0x000000000000aaaa, str_lhu_val_alt1

    la      s0, base_anchor
    li      t0, 0x5555
    sh      t0, 0(s0)
    mv      t3, s0
    lhu     a4, 0(s0)
    la      a0, str_lhu_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lhu     t4, 0(s0)
    la      a0, str_lhu_mem_unchanged
    li      a1, 0x5555
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LW : rd = sign_extend32(*(int32_t *)(rs1 + off))
 * ======================================================================= */
test_base_lw:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LW32_RD lw, ra,  0xffffffffaaaaaaaa, str_lw_rd_ra
    T_LW32_RD lw, t0,  0xffffffffaaaaaaaa, str_lw_rd_t0
    T_LW32_RD lw, s0,  0xffffffffaaaaaaaa, str_lw_rd_s0
    T_LW32_RD lw, a0,  0xffffffffaaaaaaaa, str_lw_rd_a0
    T_LW32_RD lw, s10, 0xffffffffaaaaaaaa, str_lw_rd_s10
    T_LW32_RD lw, t6,  0xffffffffaaaaaaaa, str_lw_rd_t6

    la      s0, base_anchor
    li      t0, 0xaaaaaaaa
    sw      t0, 0(s0)
    lw      zero, 0(s0)
    la      a0, str_lw_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LW32_RS1 lw, ra,  0xffffffffaaaaaaaa, str_lw_rs1_ra
    T_LW32_RS1 lw, t0,  0xffffffffaaaaaaaa, str_lw_rs1_t0
    T_LW32_RS1 lw, s0,  0xffffffffaaaaaaaa, str_lw_rs1_s0
    T_LW32_RS1 lw, a0,  0xffffffffaaaaaaaa, str_lw_rs1_a0
    T_LW32_RS1 lw, s10, 0xffffffffaaaaaaaa, str_lw_rs1_s10
    T_LW32_RS1 lw, t6,  0xffffffffaaaaaaaa, str_lw_rs1_t6

    T_LW32_OFF lw, 0,     0xffffffffaaaaaaaa, str_lw_off0
    T_LW32_OFF lw, 4,     0xffffffffaaaaaaaa, str_lw_off4
    T_LW32_OFF lw, -4,    0xffffffffaaaaaaaa, str_lw_offm4
    T_LW32_OFF lw, 8,     0xffffffffaaaaaaaa, str_lw_off8
    T_LW32_OFF lw, -8,    0xffffffffaaaaaaaa, str_lw_offm8
    T_LW32_OFF lw, 2047,  0xffffffffaaaaaaaa, str_lw_offmax
    T_LW32_OFF lw, -2048, 0xffffffffaaaaaaaa, str_lw_offmin

    T_LW32_VAL lw, 0x00000000, 0x0000000000000000, str_lw_val_zero
    T_LW32_VAL lw, 0x7fffffff, 0x000000007fffffff, str_lw_val_maxpos
    T_LW32_VAL lw, 0x80000000, 0xffffffff80000000, str_lw_val_minneg
    T_LW32_VAL lw, 0xffffffff, 0xffffffffffffffff, str_lw_val_allones
    T_LW32_VAL lw, 0x55555555, 0x0000000055555555, str_lw_val_alt0
    T_LW32_VAL lw, 0xaaaaaaaa, 0xffffffffaaaaaaaa, str_lw_val_alt1
    T_LW32_VAL lw, 0xdeadbeef, 0xffffffffdeadbeef, str_lw_val_deadbeef

    la      s0, base_anchor
    li      t0, 0x55555555
    sw      t0, 0(s0)
    mv      t3, s0
    lw      a4, 0(s0)
    la      a0, str_lw_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lwu     t4, 0(s0)
    la      a0, str_lw_mem_unchanged
    li      a1, 0x55555555
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LWU : rd = zero_extend32(*(uint32_t *)(rs1 + off))  -- RV64-only;
 *        needed because plain LW always sign-extends.
 * ======================================================================= */
test_base_lwu:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LW32_RD lwu, ra,  0x00000000aaaaaaaa, str_lwu_rd_ra
    T_LW32_RD lwu, t0,  0x00000000aaaaaaaa, str_lwu_rd_t0
    T_LW32_RD lwu, s0,  0x00000000aaaaaaaa, str_lwu_rd_s0
    T_LW32_RD lwu, a0,  0x00000000aaaaaaaa, str_lwu_rd_a0
    T_LW32_RD lwu, s10, 0x00000000aaaaaaaa, str_lwu_rd_s10
    T_LW32_RD lwu, t6,  0x00000000aaaaaaaa, str_lwu_rd_t6

    la      s0, base_anchor
    li      t0, 0xaaaaaaaa
    sw      t0, 0(s0)
    lwu     zero, 0(s0)
    la      a0, str_lwu_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LW32_RS1 lwu, ra,  0x00000000aaaaaaaa, str_lwu_rs1_ra
    T_LW32_RS1 lwu, t0,  0x00000000aaaaaaaa, str_lwu_rs1_t0
    T_LW32_RS1 lwu, s0,  0x00000000aaaaaaaa, str_lwu_rs1_s0
    T_LW32_RS1 lwu, a0,  0x00000000aaaaaaaa, str_lwu_rs1_a0
    T_LW32_RS1 lwu, s10, 0x00000000aaaaaaaa, str_lwu_rs1_s10
    T_LW32_RS1 lwu, t6,  0x00000000aaaaaaaa, str_lwu_rs1_t6

    T_LW32_OFF lwu, 0,     0x00000000aaaaaaaa, str_lwu_off0
    T_LW32_OFF lwu, 4,     0x00000000aaaaaaaa, str_lwu_off4
    T_LW32_OFF lwu, -4,    0x00000000aaaaaaaa, str_lwu_offm4
    T_LW32_OFF lwu, 8,     0x00000000aaaaaaaa, str_lwu_off8
    T_LW32_OFF lwu, -8,    0x00000000aaaaaaaa, str_lwu_offm8
    T_LW32_OFF lwu, 2047,  0x00000000aaaaaaaa, str_lwu_offmax
    T_LW32_OFF lwu, -2048, 0x00000000aaaaaaaa, str_lwu_offmin

    T_LW32_VAL lwu, 0x00000000, 0x0000000000000000, str_lwu_val_zero
    T_LW32_VAL lwu, 0x7fffffff, 0x000000007fffffff, str_lwu_val_maxpos
    T_LW32_VAL lwu, 0x80000000, 0x0000000080000000, str_lwu_val_minneg
    T_LW32_VAL lwu, 0xffffffff, 0x00000000ffffffff, str_lwu_val_allones
    T_LW32_VAL lwu, 0x55555555, 0x0000000055555555, str_lwu_val_alt0
    T_LW32_VAL lwu, 0xaaaaaaaa, 0x00000000aaaaaaaa, str_lwu_val_alt1
    T_LW32_VAL lwu, 0xdeadbeef, 0x00000000deadbeef, str_lwu_val_deadbeef

    la      s0, base_anchor
    li      t0, 0x55555555
    sw      t0, 0(s0)
    mv      t3, s0
    lwu     a4, 0(s0)
    la      a0, str_lwu_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lwu     t4, 0(s0)
    la      a0, str_lwu_mem_unchanged
    li      a1, 0x55555555
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  LD : rd = *(uint64_t *)(rs1 + off)  -- exact, no extension question.
 * ======================================================================= */
test_base_ld:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    T_LD64_RD ra,  str_ld_rd_ra
    T_LD64_RD t0,  str_ld_rd_t0
    T_LD64_RD s0,  str_ld_rd_s0
    T_LD64_RD a0,  str_ld_rd_a0
    T_LD64_RD s10, str_ld_rd_s10
    T_LD64_RD t6,  str_ld_rd_t6

    la      s0, base_anchor
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, 0(s0)
    ld      zero, 0(s0)
    la      a0, str_ld_rd_x0
    li      a1, 0
    mv      a2, zero
    jal     ra, check

    T_LD64_RS1 ra,  str_ld_rs1_ra
    T_LD64_RS1 t0,  str_ld_rs1_t0
    T_LD64_RS1 s0,  str_ld_rs1_s0
    T_LD64_RS1 a0,  str_ld_rs1_a0
    T_LD64_RS1 s10, str_ld_rs1_s10
    T_LD64_RS1 t6,  str_ld_rs1_t6

    T_LD64_OFF 0,     str_ld_off0
    T_LD64_OFF 4,     str_ld_off4
    T_LD64_OFF -4,    str_ld_offm4
    T_LD64_OFF 8,     str_ld_off8
    T_LD64_OFF -8,    str_ld_offm8
    T_LD64_OFF 2039,  str_ld_offmax    /* 2047 minus 8 bytes of load width */
    T_LD64_OFF -2048, str_ld_offmin

    T_LD64_VAL 0x0000000000000000, str_ld_val_zero
    T_LD64_VAL 0x7fffffffffffffff, str_ld_val_maxpos
    T_LD64_VAL 0x8000000000000000, str_ld_val_minneg
    T_LD64_VAL 0xffffffffffffffff, str_ld_val_allones
    T_LD64_VAL 0x5555555555555555, str_ld_val_alt0
    T_LD64_VAL 0xaaaaaaaaaaaaaaaa, str_ld_val_alt1
    T_LD64_VAL 0x0123456789abcdef, str_ld_val_walking
    T_LD64_VAL 0x0000000000000001, str_ld_val_one

    la      s0, base_anchor
    li      t0, 0x0123456789abcdef
    sd      t0, 0(s0)
    mv      t3, s0
    ld      a4, 0(s0)
    la      a0, str_ld_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    ld      t4, 0(s0)
    la      a0, str_ld_mem_unchanged
    li      a1, 0x0123456789abcdef
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  Entry point for this file
 * ======================================================================= */
tests_base_loads:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    jal     ra, test_base_lb
    jal     ra, test_base_lbu
    jal     ra, test_base_lh
    jal     ra, test_base_lhu
    jal     ra, test_base_lw
    jal     ra, test_base_lwu
    jal     ra, test_base_ld

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  Strings
 * ======================================================================= */
    .section .rodata
str_lb_rd_ra:  .asciz "LB rd=ra(x1)"
str_lb_rd_t0:  .asciz "LB rd=t0(x5)"
str_lb_rd_s0:  .asciz "LB rd=s0(x8)"
str_lb_rd_a0:  .asciz "LB rd=a0(x10)"
str_lb_rd_s10: .asciz "LB rd=s10(x26)"
str_lb_rd_t6:  .asciz "LB rd=t6(x31)"
str_lb_rd_x0:  .asciz "LB rd=x0 discards result, no fault"
str_lb_rs1_ra:  .asciz "LB rs1=ra(x1)"
str_lb_rs1_t0:  .asciz "LB rs1=t0(x5)"
str_lb_rs1_s0:  .asciz "LB rs1=s0(x8)"
str_lb_rs1_a0:  .asciz "LB rs1=a0(x10)"
str_lb_rs1_s10: .asciz "LB rs1=s10(x26)"
str_lb_rs1_t6:  .asciz "LB rs1=t6(x31)"
str_lb_off0:   .asciz "LB offset=0"
str_lb_off4:   .asciz "LB offset=+4"
str_lb_offm4:  .asciz "LB offset=-4"
str_lb_off8:   .asciz "LB offset=+8"
str_lb_offm8:  .asciz "LB offset=-8"
str_lb_offmax: .asciz "LB offset=+2047 (max)"
str_lb_offmin: .asciz "LB offset=-2048 (min)"
str_lb_val_zero:    .asciz "LB value=0x00"
str_lb_val_maxpos:  .asciz "LB value=0x7f (no sign-ext)"
str_lb_val_minneg:  .asciz "LB value=0x80 (sign-extends)"
str_lb_val_allones: .asciz "LB value=0xff (sign-extends)"
str_lb_val_alt0:    .asciz "LB value=0x55 (no sign-ext)"
str_lb_val_alt1:    .asciz "LB value=0xaa (sign-extends)"
str_lb_rs1_preserved: .asciz "LB rs1 unmodified"
str_lb_mem_unchanged: .asciz "LB memory unmodified (pure read)"

str_lbu_rd_ra:  .asciz "LBU rd=ra(x1)"
str_lbu_rd_t0:  .asciz "LBU rd=t0(x5)"
str_lbu_rd_s0:  .asciz "LBU rd=s0(x8)"
str_lbu_rd_a0:  .asciz "LBU rd=a0(x10)"
str_lbu_rd_s10: .asciz "LBU rd=s10(x26)"
str_lbu_rd_t6:  .asciz "LBU rd=t6(x31)"
str_lbu_rd_x0:  .asciz "LBU rd=x0 discards result, no fault"
str_lbu_rs1_ra:  .asciz "LBU rs1=ra(x1)"
str_lbu_rs1_t0:  .asciz "LBU rs1=t0(x5)"
str_lbu_rs1_s0:  .asciz "LBU rs1=s0(x8)"
str_lbu_rs1_a0:  .asciz "LBU rs1=a0(x10)"
str_lbu_rs1_s10: .asciz "LBU rs1=s10(x26)"
str_lbu_rs1_t6:  .asciz "LBU rs1=t6(x31)"
str_lbu_off0:   .asciz "LBU offset=0"
str_lbu_off4:   .asciz "LBU offset=+4"
str_lbu_offm4:  .asciz "LBU offset=-4"
str_lbu_off8:   .asciz "LBU offset=+8"
str_lbu_offm8:  .asciz "LBU offset=-8"
str_lbu_offmax: .asciz "LBU offset=+2047 (max)"
str_lbu_offmin: .asciz "LBU offset=-2048 (min)"
str_lbu_val_zero:    .asciz "LBU value=0x00"
str_lbu_val_maxpos:  .asciz "LBU value=0x7f"
str_lbu_val_minneg:  .asciz "LBU value=0x80 (no zero-ext confusion)"
str_lbu_val_allones: .asciz "LBU value=0xff (zero-extends, not -1)"
str_lbu_val_alt0:    .asciz "LBU value=0x55"
str_lbu_val_alt1:    .asciz "LBU value=0xaa (zero-extends)"
str_lbu_rs1_preserved: .asciz "LBU rs1 unmodified"
str_lbu_mem_unchanged: .asciz "LBU memory unmodified (pure read)"

str_lh_rd_ra:  .asciz "LH rd=ra(x1)"
str_lh_rd_t0:  .asciz "LH rd=t0(x5)"
str_lh_rd_s0:  .asciz "LH rd=s0(x8)"
str_lh_rd_a0:  .asciz "LH rd=a0(x10)"
str_lh_rd_s10: .asciz "LH rd=s10(x26)"
str_lh_rd_t6:  .asciz "LH rd=t6(x31)"
str_lh_rd_x0:  .asciz "LH rd=x0 discards result, no fault"
str_lh_rs1_ra:  .asciz "LH rs1=ra(x1)"
str_lh_rs1_t0:  .asciz "LH rs1=t0(x5)"
str_lh_rs1_s0:  .asciz "LH rs1=s0(x8)"
str_lh_rs1_a0:  .asciz "LH rs1=a0(x10)"
str_lh_rs1_s10: .asciz "LH rs1=s10(x26)"
str_lh_rs1_t6:  .asciz "LH rs1=t6(x31)"
str_lh_off0:   .asciz "LH offset=0"
str_lh_off4:   .asciz "LH offset=+4"
str_lh_offm4:  .asciz "LH offset=-4"
str_lh_off8:   .asciz "LH offset=+8"
str_lh_offm8:  .asciz "LH offset=-8"
str_lh_offmax: .asciz "LH offset=+2047 (max)"
str_lh_offmin: .asciz "LH offset=-2048 (min)"
str_lh_val_zero:    .asciz "LH value=0x0000"
str_lh_val_maxpos:  .asciz "LH value=0x7fff (no sign-ext)"
str_lh_val_minneg:  .asciz "LH value=0x8000 (sign-extends)"
str_lh_val_allones: .asciz "LH value=0xffff (sign-extends)"
str_lh_val_alt0:    .asciz "LH value=0x5555 (no sign-ext)"
str_lh_val_alt1:    .asciz "LH value=0xaaaa (sign-extends)"
str_lh_rs1_preserved: .asciz "LH rs1 unmodified"
str_lh_mem_unchanged: .asciz "LH memory unmodified (pure read)"

str_lhu_rd_ra:  .asciz "LHU rd=ra(x1)"
str_lhu_rd_t0:  .asciz "LHU rd=t0(x5)"
str_lhu_rd_s0:  .asciz "LHU rd=s0(x8)"
str_lhu_rd_a0:  .asciz "LHU rd=a0(x10)"
str_lhu_rd_s10: .asciz "LHU rd=s10(x26)"
str_lhu_rd_t6:  .asciz "LHU rd=t6(x31)"
str_lhu_rd_x0:  .asciz "LHU rd=x0 discards result, no fault"
str_lhu_rs1_ra:  .asciz "LHU rs1=ra(x1)"
str_lhu_rs1_t0:  .asciz "LHU rs1=t0(x5)"
str_lhu_rs1_s0:  .asciz "LHU rs1=s0(x8)"
str_lhu_rs1_a0:  .asciz "LHU rs1=a0(x10)"
str_lhu_rs1_s10: .asciz "LHU rs1=s10(x26)"
str_lhu_rs1_t6:  .asciz "LHU rs1=t6(x31)"
str_lhu_off0:   .asciz "LHU offset=0"
str_lhu_off4:   .asciz "LHU offset=+4"
str_lhu_offm4:  .asciz "LHU offset=-4"
str_lhu_off8:   .asciz "LHU offset=+8"
str_lhu_offm8:  .asciz "LHU offset=-8"
str_lhu_offmax: .asciz "LHU offset=+2047 (max)"
str_lhu_offmin: .asciz "LHU offset=-2048 (min)"
str_lhu_val_zero:    .asciz "LHU value=0x0000"
str_lhu_val_maxpos:  .asciz "LHU value=0x7fff"
str_lhu_val_minneg:  .asciz "LHU value=0x8000"
str_lhu_val_allones: .asciz "LHU value=0xffff (zero-extends, not -1)"
str_lhu_val_alt0:    .asciz "LHU value=0x5555"
str_lhu_val_alt1:    .asciz "LHU value=0xaaaa (zero-extends)"
str_lhu_rs1_preserved: .asciz "LHU rs1 unmodified"
str_lhu_mem_unchanged: .asciz "LHU memory unmodified (pure read)"

str_lw_rd_ra:  .asciz "LW rd=ra(x1)"
str_lw_rd_t0:  .asciz "LW rd=t0(x5)"
str_lw_rd_s0:  .asciz "LW rd=s0(x8)"
str_lw_rd_a0:  .asciz "LW rd=a0(x10)"
str_lw_rd_s10: .asciz "LW rd=s10(x26)"
str_lw_rd_t6:  .asciz "LW rd=t6(x31)"
str_lw_rd_x0:  .asciz "LW rd=x0 discards result, no fault"
str_lw_rs1_ra:  .asciz "LW rs1=ra(x1)"
str_lw_rs1_t0:  .asciz "LW rs1=t0(x5)"
str_lw_rs1_s0:  .asciz "LW rs1=s0(x8)"
str_lw_rs1_a0:  .asciz "LW rs1=a0(x10)"
str_lw_rs1_s10: .asciz "LW rs1=s10(x26)"
str_lw_rs1_t6:  .asciz "LW rs1=t6(x31)"
str_lw_off0:   .asciz "LW offset=0"
str_lw_off4:   .asciz "LW offset=+4"
str_lw_offm4:  .asciz "LW offset=-4"
str_lw_off8:   .asciz "LW offset=+8"
str_lw_offm8:  .asciz "LW offset=-8"
str_lw_offmax: .asciz "LW offset=+2047 (max)"
str_lw_offmin: .asciz "LW offset=-2048 (min)"
str_lw_val_zero:     .asciz "LW value=0x00000000"
str_lw_val_maxpos:   .asciz "LW value=0x7fffffff (no sign-ext)"
str_lw_val_minneg:   .asciz "LW value=0x80000000 (sign-extends)"
str_lw_val_allones:  .asciz "LW value=0xffffffff (sign-extends)"
str_lw_val_alt0:     .asciz "LW value=0x55555555 (no sign-ext)"
str_lw_val_alt1:     .asciz "LW value=0xaaaaaaaa (sign-extends)"
str_lw_val_deadbeef: .asciz "LW value=0xdeadbeef (sign-extends)"
str_lw_rs1_preserved: .asciz "LW rs1 unmodified"
str_lw_mem_unchanged: .asciz "LW memory unmodified (pure read)"

str_lwu_rd_ra:  .asciz "LWU rd=ra(x1)"
str_lwu_rd_t0:  .asciz "LWU rd=t0(x5)"
str_lwu_rd_s0:  .asciz "LWU rd=s0(x8)"
str_lwu_rd_a0:  .asciz "LWU rd=a0(x10)"
str_lwu_rd_s10: .asciz "LWU rd=s10(x26)"
str_lwu_rd_t6:  .asciz "LWU rd=t6(x31)"
str_lwu_rd_x0:  .asciz "LWU rd=x0 discards result, no fault"
str_lwu_rs1_ra:  .asciz "LWU rs1=ra(x1)"
str_lwu_rs1_t0:  .asciz "LWU rs1=t0(x5)"
str_lwu_rs1_s0:  .asciz "LWU rs1=s0(x8)"
str_lwu_rs1_a0:  .asciz "LWU rs1=a0(x10)"
str_lwu_rs1_s10: .asciz "LWU rs1=s10(x26)"
str_lwu_rs1_t6:  .asciz "LWU rs1=t6(x31)"
str_lwu_off0:   .asciz "LWU offset=0"
str_lwu_off4:   .asciz "LWU offset=+4"
str_lwu_offm4:  .asciz "LWU offset=-4"
str_lwu_off8:   .asciz "LWU offset=+8"
str_lwu_offm8:  .asciz "LWU offset=-8"
str_lwu_offmax: .asciz "LWU offset=+2047 (max)"
str_lwu_offmin: .asciz "LWU offset=-2048 (min)"
str_lwu_val_zero:     .asciz "LWU value=0x00000000"
str_lwu_val_maxpos:   .asciz "LWU value=0x7fffffff"
str_lwu_val_minneg:   .asciz "LWU value=0x80000000 (zero-extends, not negative)"
str_lwu_val_allones:  .asciz "LWU value=0xffffffff (zero-extends, not -1)"
str_lwu_val_alt0:     .asciz "LWU value=0x55555555"
str_lwu_val_alt1:     .asciz "LWU value=0xaaaaaaaa (zero-extends)"
str_lwu_val_deadbeef: .asciz "LWU value=0xdeadbeef (zero-extends)"
str_lwu_rs1_preserved: .asciz "LWU rs1 unmodified"
str_lwu_mem_unchanged: .asciz "LWU memory unmodified (pure read)"

str_ld_rd_ra:  .asciz "LD rd=ra(x1)"
str_ld_rd_t0:  .asciz "LD rd=t0(x5)"
str_ld_rd_s0:  .asciz "LD rd=s0(x8)"
str_ld_rd_a0:  .asciz "LD rd=a0(x10)"
str_ld_rd_s10: .asciz "LD rd=s10(x26)"
str_ld_rd_t6:  .asciz "LD rd=t6(x31)"
str_ld_rd_x0:  .asciz "LD rd=x0 discards result, no fault"
str_ld_rs1_ra:  .asciz "LD rs1=ra(x1)"
str_ld_rs1_t0:  .asciz "LD rs1=t0(x5)"
str_ld_rs1_s0:  .asciz "LD rs1=s0(x8)"
str_ld_rs1_a0:  .asciz "LD rs1=a0(x10)"
str_ld_rs1_s10: .asciz "LD rs1=s10(x26)"
str_ld_rs1_t6:  .asciz "LD rs1=t6(x31)"
str_ld_off0:   .asciz "LD offset=0"
str_ld_off4:   .asciz "LD offset=+4"
str_ld_offm4:  .asciz "LD offset=-4"
str_ld_off8:   .asciz "LD offset=+8"
str_ld_offm8:  .asciz "LD offset=-8"
str_ld_offmax: .asciz "LD offset=+2039 (max minus load width)"
str_ld_offmin: .asciz "LD offset=-2048 (min)"
str_ld_val_zero:    .asciz "LD value=0x0000000000000000"
str_ld_val_maxpos:  .asciz "LD value=0x7fffffffffffffff"
str_ld_val_minneg:  .asciz "LD value=0x8000000000000000"
str_ld_val_allones: .asciz "LD value=0xffffffffffffffff"
str_ld_val_alt0:    .asciz "LD value=0x5555555555555555"
str_ld_val_alt1:    .asciz "LD value=0xaaaaaaaaaaaaaaaa"
str_ld_val_walking: .asciz "LD value=0x0123456789abcdef"
str_ld_val_one:     .asciz "LD value=0x0000000000000001"
str_ld_rs1_preserved: .asciz "LD rs1 unmodified"
str_ld_mem_unchanged: .asciz "LD memory unmodified (pure read)"

/* =======================================================================
 *  Data buffer: base_anchor sits in the middle so both +2047 and
 *  -2048 (the full 12-bit signed I-type immediate range) stay inside
 *  the allocated region, with margin for the width of the load itself.
 * ======================================================================= */
    .section .bss
    .align 3
    .space 2304
base_anchor:
    .space 2304
