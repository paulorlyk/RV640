/* =====================================================================
 * base_jalr.S  --  RV64I base-ISA JALR (jump and link register)
 *
 * Part of the RV64I base-ISA suite; see base_tests.S for how this
 * plugs into the rest (main_tests.S -> base_tests.S -> here), and
 * common.S for the boot/UART/reporting harness everything sits on
 * ("check", "uart_puts" are used from there; nothing else).
 *
 * JALR is I-type: target = (rs1 + sign_extend(imm12)) & ~1, and
 * rd = pc + 4. Three things make it meaningfully different from JAL
 * (base_jal.S) and worth its own distinct coverage rather than a
 * copy of that file's structure:
 *
 *   1. The target is register-relative, not PC-relative, so the
 *      12-bit immediate is a plain contiguous field (no scrambling)
 *      and -- unlike JAL's ~1MB PC-relative range -- the full
 *      -2048..+2047 range is cheap to sweep exhaustively at the
 *      boundaries without constructing enormous filler runs. Each
 *      immediate case computes its base register at runtime as
 *      (target_label - imm), so the jump lands on the label for any
 *      immediate under test.
 *   2. The low bit of the computed target is *cleared* (& ~1), not
 *      preserved. This is easy for an implementation to get wrong and
 *      is tested directly: a base register deliberately set to
 *      (label | 1) must still land exactly on label.
 *   3. rd and rs1 are independent full 5-bit fields, so rd == rs1 is
 *      a genuine hazard case: the old rs1 value must be read as the
 *      jump target *before* rd is overwritten with the link address.
 *      An implementation that writes the link first would jump to the
 *      link address instead. Tested explicitly.
 *
 * rd=x0 (discarding the link -- the standard "computed/indirect jump"
 * and "return" idiom) is tested as a first-class case. Note this file
 * uses `jalr rd, rs1, imm` three-operand form throughout rather than
 * the `ret` / `jr` pseudo-instructions, so what's being tested is
 * unambiguous.
 * ===================================================================== */

    .option norvc            /* mandatory, not stylistic: JALR *does* have
                               * compressed forms (C.JR / C.JALR) that the
                               * assembler would otherwise substitute for
                               * imm=0 cases, which would silently test the
                               * RVC instruction instead of this one. */

    .global tests_base_jalr

    .section .text

/* T_JALR_REG rdreg, name_landed, name_link -- sweep rd. The base
 * register is fixed at s3 (deliberately not in the swept register
 * list, and not clobbered by check(), which only touches t0-t6 and
 * a0-a2). Checks landing and link value independently. rdreg may be
 * a0/a1/a2, so the link value is captured into s5 before check()
 * runs. */
.macro T_JALR_REG rdreg, name_landed, name_link
    li      s4, 0
    la      s3, 1f
.Ljalr_reg\@:
    jalr    \rdreg, s3, 0
    li      s4, 999                 /* poison: only hit if JALR failed to jump */
1:
    mv      s5, \rdreg              /* capture link before check() runs */
    la      a0, \name_landed
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    la      t3, .Ljalr_reg\@
    addi    t3, t3, 4               /* expected link = this JALR's addr + 4 */
    la      a0, \name_link
    mv      a1, t3
    mv      a2, s5
    jal     ra, check
.endm

/* T_JALR_RS1 basereg, name -- sweep rs1 (the base register), rd fixed
 * at x0 so the link plays no part here. basereg may be a0/a1/a2: it's
 * fully consumed by the jump before a0-a2 are reused. */
.macro T_JALR_RS1 basereg, name
    li      s4, 0
    la      \basereg, 1f
    jalr    zero, \basereg, 0
    li      s4, 999                 /* poison: only hit if JALR failed to jump */
1:
    la      a0, \name
    li      a1, 0
    mv      a2, s4
    jal     ra, check
.endm

/* T_JALR_IMM imm, name -- sweep the 12-bit signed immediate. The base
 * is computed at runtime as (target - imm) so the jump lands on the
 * target label for whatever immediate is under test. The subtraction
 * uses li+sub rather than `addi s3, s3, -\imm` because negating the
 * immediate can leave the -(-2048) = +2048 case outside addi's own
 * -2048..+2047 range. */
.macro T_JALR_IMM imm, name
    li      s4, 0
    la      s3, 1f
    li      t0, \imm
    sub     s3, s3, t0              /* base = target - imm */
    jalr    zero, s3, \imm          /* ... so base + imm == target */
    li      s4, 999                 /* poison: only hit if JALR landed wrong */
1:
    la      a0, \name
    li      a1, 0
    mv      a2, s4
    jal     ra, check
.endm

tests_base_jalr:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every register in the full 5-bit rd field --- */
    T_JALR_REG ra,  str_jalr_landed_ra,  str_jalr_link_ra
    T_JALR_REG t0,  str_jalr_landed_t0,  str_jalr_link_t0
    T_JALR_REG s0,  str_jalr_landed_s0,  str_jalr_link_s0
    T_JALR_REG a0,  str_jalr_landed_a0,  str_jalr_link_a0
    T_JALR_REG s10, str_jalr_landed_s10, str_jalr_link_s10
    T_JALR_REG t6,  str_jalr_landed_t6,  str_jalr_link_t6

    /* --- rd=x0: the standard indirect-jump / return idiom --- */
    li      s4, 0
    la      s3, 1f
    jalr    zero, s3, 0
    li      s4, 999
1:
    la      a0, str_jalr_rd_x0
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    /* --- every register in the full 5-bit rs1 field --- */
    T_JALR_RS1 ra,  str_jalr_rs1_ra
    T_JALR_RS1 t0,  str_jalr_rs1_t0
    T_JALR_RS1 s0,  str_jalr_rs1_s0
    T_JALR_RS1 a0,  str_jalr_rs1_a0
    T_JALR_RS1 s10, str_jalr_rs1_s10
    T_JALR_RS1 t6,  str_jalr_rs1_t6

    /* --- the full 12-bit signed immediate range: zero, both
     *     boundaries, every individual magnitude bit in both
     *     directions, and the sign-bit boundary pair --- */
    T_JALR_IMM 0,     str_jalr_imm_zero
    T_JALR_IMM 4,     str_jalr_imm_p4
    T_JALR_IMM -4,    str_jalr_imm_m4
    T_JALR_IMM 8,     str_jalr_imm_p8
    T_JALR_IMM -8,    str_jalr_imm_m8
    T_JALR_IMM 16,    str_jalr_imm_p16
    T_JALR_IMM -16,   str_jalr_imm_m16
    T_JALR_IMM 64,    str_jalr_imm_p64
    T_JALR_IMM -64,   str_jalr_imm_m64
    T_JALR_IMM 256,   str_jalr_imm_p256
    T_JALR_IMM -256,  str_jalr_imm_m256
    T_JALR_IMM 1024,  str_jalr_imm_p1024
    T_JALR_IMM -1024, str_jalr_imm_m1024
    T_JALR_IMM 2046,  str_jalr_imm_maxeven
    T_JALR_IMM 2047,  str_jalr_imm_max
    T_JALR_IMM -2048, str_jalr_imm_min

    /* --- the low bit of the computed target must be CLEARED, not
     *     preserved: base deliberately set to (target | 1), which must
     *     still land exactly on target. This is the behaviour that
     *     actually distinguishes JALR's address computation from a
     *     plain register-indirect jump. --- */
    li      s4, 0
    la      s3, 1f
    ori     s3, s3, 1               /* deliberately dirty the low bit */
    jalr    zero, s3, 0
    li      s4, 999
1:
    la      a0, str_jalr_lsb_cleared
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    /* --- same, but with the odd bit arriving via the immediate
     *     rather than the base register --- */
    li      s4, 0
    la      s3, 1f
    addi    s3, s3, -3
    jalr    zero, s3, 3             /* base+imm == target+1, low bit cleared */
    li      s4, 999
1:
    la      a0, str_jalr_lsb_cleared_imm
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    /* --- rd == rs1 aliasing: the old rs1 must be read as the jump
     *     target *before* rd is overwritten with the link address. An
     *     implementation that writes the link first would jump to the
     *     link address instead of the intended target. --- */
    li      s4, 0
    la      t1, 1f
.Ljalr_alias:
    jalr    t1, t1, 0               /* rd == rs1 == t1 */
    li      s4, 999
1:
    mv      s5, t1                  /* capture link before check() runs */
    la      a0, str_jalr_alias_landed
    li      a1, 0
    mv      a2, s4
    jal     ra, check

    la      t3, .Ljalr_alias
    addi    t3, t3, 4
    la      a0, str_jalr_alias_link
    mv      a1, t3
    mv      a2, s5
    jal     ra, check

    /* --- a real call/return round trip through JALR in both roles:
     *     JALR to call a subroutine, JALR to return from it --- */
    li      s4, 0
    la      s3, jalr_callee
    jalr    ra, s3, 0               /* call */
    la      a0, str_jalr_call_return
    li      a1, 1
    mv      a2, s4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* Callee for the call/return round-trip test above. Sets the marker,
 * then returns via JALR (not the `ret` pseudo-instruction, so the
 * instruction under test is explicit). */
jalr_callee:
    li      s4, 1
    jalr    zero, ra, 0

    .section .rodata
str_jalr_landed_ra:  .asciz "JALR rd=ra(x1) landed"
str_jalr_landed_t0:  .asciz "JALR rd=t0(x5) landed"
str_jalr_landed_s0:  .asciz "JALR rd=s0(x8) landed"
str_jalr_landed_a0:  .asciz "JALR rd=a0(x10) landed"
str_jalr_landed_s10: .asciz "JALR rd=s10(x26) landed"
str_jalr_landed_t6:  .asciz "JALR rd=t6(x31) landed"
str_jalr_link_ra:  .asciz "JALR rd=ra(x1) link value correct (pc+4)"
str_jalr_link_t0:  .asciz "JALR rd=t0(x5) link value correct (pc+4)"
str_jalr_link_s0:  .asciz "JALR rd=s0(x8) link value correct (pc+4)"
str_jalr_link_a0:  .asciz "JALR rd=a0(x10) link value correct (pc+4)"
str_jalr_link_s10: .asciz "JALR rd=s10(x26) link value correct (pc+4)"
str_jalr_link_t6:  .asciz "JALR rd=t6(x31) link value correct (pc+4)"
str_jalr_rd_x0:    .asciz "JALR rd=x0 (indirect jump / return idiom)"
str_jalr_rs1_ra:  .asciz "JALR rs1=ra(x1)"
str_jalr_rs1_t0:  .asciz "JALR rs1=t0(x5)"
str_jalr_rs1_s0:  .asciz "JALR rs1=s0(x8)"
str_jalr_rs1_a0:  .asciz "JALR rs1=a0(x10)"
str_jalr_rs1_s10: .asciz "JALR rs1=s10(x26)"
str_jalr_rs1_t6:  .asciz "JALR rs1=t6(x31)"
str_jalr_imm_zero:   .asciz "JALR imm=0"
str_jalr_imm_p4:     .asciz "JALR imm=+4 (bit2)"
str_jalr_imm_m4:     .asciz "JALR imm=-4"
str_jalr_imm_p8:     .asciz "JALR imm=+8 (bit3)"
str_jalr_imm_m8:     .asciz "JALR imm=-8"
str_jalr_imm_p16:    .asciz "JALR imm=+16 (bit4)"
str_jalr_imm_m16:    .asciz "JALR imm=-16"
str_jalr_imm_p64:    .asciz "JALR imm=+64 (bit6)"
str_jalr_imm_m64:    .asciz "JALR imm=-64"
str_jalr_imm_p256:   .asciz "JALR imm=+256 (bit8)"
str_jalr_imm_m256:   .asciz "JALR imm=-256"
str_jalr_imm_p1024:  .asciz "JALR imm=+1024 (bit10)"
str_jalr_imm_m1024:  .asciz "JALR imm=-1024"
str_jalr_imm_maxeven: .asciz "JALR imm=+2046 (max even)"
str_jalr_imm_max:    .asciz "JALR imm=+2047 (max, odd)"
str_jalr_imm_min:    .asciz "JALR imm=-2048 (min)"
str_jalr_lsb_cleared:     .asciz "JALR clears low bit of target (odd base)"
str_jalr_lsb_cleared_imm: .asciz "JALR clears low bit of target (odd via imm)"
str_jalr_alias_landed: .asciz "JALR rd==rs1 alias landed (reads rs1 before writing rd)"
str_jalr_alias_link:   .asciz "JALR rd==rs1 alias link value correct (pc+4)"
str_jalr_call_return:  .asciz "JALR call/return round trip"
