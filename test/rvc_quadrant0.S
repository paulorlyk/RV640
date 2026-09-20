/* =====================================================================
 * rvc_quadrant0.S  --  RVC quadrant 0 (compressed opcode bits[1:0]=00):
 *                       C.ADDI4SPN, C.LW, C.SW, C.LD, C.SD
 *
 * Part of the RV64C test suite; see rvc_tests.S for the overall
 * structure and common.S for the boot/UART/reporting harness this
 * builds on ("check", "uart_puts", etc. are provided there). This
 * file's only entry point called from outside is tests_quadrant0,
 * invoked once by run_tests (in rvc_tests.S).
 *
 * word_buf/dword_buf (defined here) are also used by rvc_quadrant1.S's
 * C.NOP memory-unaffected check, hence .global.
 * ===================================================================== */

    .option norvc            /* c.* instructions are wrapped in their own */
                              /* .option rvc / .option norvc pair so every */
                              /* compressed encoding is deliberate. */

    .global tests_quadrant0
    .global word_buf
    .global dword_buf

    .section .text

/* =======================================================================
 *  Reusable per-instruction test macros
 *
 *  These are intentionally narrow (one instruction family at a time) so
 *  that each instruction's edge cases -- every legal destination
 *  register, every individual immediate-field bit, boundary values,
 *  and any documented invariants (e.g. "this instruction must not
 *  modify sp") -- can be covered without hand-duplicating the same
 *  seven lines of setup/compare/report over and over.
 * ======================================================================= */

/* T_A4SPN_REG rdreg, name
 *   Exercise C.ADDI4SPN with a fixed immediate (4, the minimum legal
 *   value) into a specific destination register. Used to sweep all
 *   eight legal rd' encodings (x8-x15). rdreg may itself be a0/a1/a2
 *   (which "check" also uses as arguments) -- the result is copied out
 *   to t2 immediately, before a0-a2 are repurposed, so this is safe.
 */
.macro T_A4SPN_REG rdreg, name
    mv      t1, sp
    addi    t1, t1, 4
    .option rvc
    c.addi4spn \rdreg, sp, 4
    .option norvc
    mv      t2, \rdreg
    la      a0, \name
    mv      a1, t1
    mv      a2, t2
    jal     ra, check
.endm

/* T_A4SPN_IMM imm, name
 *   Exercise C.ADDI4SPN with a fixed destination (s0) and a specific
 *   immediate. Used to walk every individual bit of the scrambled
 *   nzuimm field (imm[9:6|5:4|2|3] in the encoding) plus the min/max
 *   boundary and a couple of mixed bit patterns, to catch bit-swizzle
 *   bugs that a single "one nice round number" test would miss.
 */
.macro T_A4SPN_IMM imm, name
    mv      t1, sp
    addi    t1, t1, \imm
    .option rvc
    c.addi4spn s0, sp, \imm
    .option norvc
    la      a0, \name
    mv      a1, t1
    mv      a2, s0
    jal     ra, check
.endm

/* T_A4SPN_CROSS rdreg, imm, name
 *   The bit-independence defense: T_A4SPN_REG and T_A4SPN_IMM above
 *   each vary *one* field while holding the other fixed, which can
 *   never expose a bug where a specific bit of nzuimm only misbehaves
 *   for specific values of rd' (e.g. a decoder wiring fault where
 *   nzuimm bit 2 leaks into the logic that also selects rd', so the
 *   corruption only shows up for some register encodings and not
 *   others). This macro is the fix: every individual nzuimm bit,
 *   crossed against every legal rd', so no (bit, register) pairing
 *   goes unchecked. Same a0/a1/a2 safety as T_A4SPN_REG.
 */
.macro T_A4SPN_CROSS rdreg, imm, name
    mv      t1, sp
    addi    t1, t1, \imm
    .option rvc
    c.addi4spn \rdreg, sp, \imm
    .option norvc
    mv      t2, \rdreg
    la      a0, \name
    mv      a1, t1
    mv      a2, t2
    jal     ra, check
.endm

/* T_A4SPN_PAIR imm, name
 *   The complementary defense *within* the nzuimm field itself: fixed
 *   rd'=s0, but the immediate under test has *two* individual bits set
 *   together (every one of the C(8,2)=28 possible pairs among the 8
 *   encodable bit positions), rather than one bit alone or all of them
 *   at once. A coupling bug between two specific nzuimm bits -- as
 *   opposed to a whole-field or single-bit fault -- would only show up
 *   when exactly that pair is set simultaneously, which neither a
 *   single-bit sweep nor the earlier "all bits" (1020) case is
 *   guaranteed to exercise.
 */
.macro T_A4SPN_PAIR imm, name
    mv      t1, sp
    addi    t1, t1, \imm
    .option rvc
    c.addi4spn s0, sp, \imm
    .option norvc
    la      a0, \name
    mv      a1, t1
    mv      a2, s0
    jal     ra, check
.endm

/* =======================================================================
 *  C.ADDI4SPN : rd' = sp + nzuimm  (CIW format, rd' in x8-x15 only,
 *               nzuimm is an unsigned multiple of 4 in [4, 1020])
 *
 *  Coverage here goes beyond independent per-field sweeps. The
 *  original register sweep (T_A4SPN_REG) and immediate sweep
 *  (T_A4SPN_IMM) below each vary exactly one of the instruction's two
 *  variable fields (rd', nzuimm) while holding the other fixed -- which
 *  is enough to catch a bug confined *within* one field, but is blind
 *  to any bug where a bit's effect depends on what the *other* field
 *  currently holds. A concrete example: a decoder fault where nzuimm
 *  bit 2 (instruction bit 6) is incorrectly OR'd into bit 1 of the
 *  instruction's own opcode field, corrupting quadrant selection --
 *  such a fault could easily be masked for one particular rd' encoding
 *  (e.g. rd'=s0, the only register the immediate sweep ever uses)
 *  while still being wrong for every other one.
 *
 *  T_A4SPN_CROSS closes that gap with a genuine cross-product: every
 *  individual nzuimm bit position, crossed against every legal rd'
 *  register (8 x 11 = 88 cases). T_A4SPN_PAIR adds the complementary
 *  check *within* the nzuimm field: every pair of individual bits set
 *  together (28 cases), which neither a single-bit sweep nor the
 *  "all bits" boundary case is guaranteed to exercise. Together these
 *  are the general defense against "any bit of any variable part of
 *  the instruction can be corrupt [and only manifest in combination
 *  with some other bit]" -- not just a defense against the one
 *  bit2/opcode-bit1 example that motivated them.
 * ======================================================================= */
test_c_addi4spn:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every legal destination register, fixed immediate --- */
    T_A4SPN_REG s0, str_a4spn_reg_s0
    T_A4SPN_REG s1, str_a4spn_reg_s1
    T_A4SPN_REG a0, str_a4spn_reg_a0
    T_A4SPN_REG a1, str_a4spn_reg_a1
    T_A4SPN_REG a2, str_a4spn_reg_a2
    T_A4SPN_REG a3, str_a4spn_reg_a3
    T_A4SPN_REG a4, str_a4spn_reg_a4
    T_A4SPN_REG a5, str_a4spn_reg_a5

    /* --- every individual immediate-field bit, isolated --- */
    T_A4SPN_IMM 4,    str_a4spn_imm4      /* bit 2 (minimum legal value) */
    T_A4SPN_IMM 8,    str_a4spn_imm8      /* bit 3 */
    T_A4SPN_IMM 16,   str_a4spn_imm16     /* bit 4 */
    T_A4SPN_IMM 32,   str_a4spn_imm32     /* bit 5 */
    T_A4SPN_IMM 64,   str_a4spn_imm64     /* bit 6 */
    T_A4SPN_IMM 128,  str_a4spn_imm128    /* bit 7 */
    T_A4SPN_IMM 256,  str_a4spn_imm256    /* bit 8 */
    T_A4SPN_IMM 512,  str_a4spn_imm512    /* bit 9 (maximum single bit) */
    T_A4SPN_IMM 1020, str_a4spn_imm1020   /* all 8 field bits set (max) */

    /* --- mixed alternating-bit patterns, to catch cross-wiring between
     *     the scattered imm[9:6|5:4|2|3] encoding fields --- */
    T_A4SPN_IMM 340,  str_a4spn_imm340    /* nzuimm/4 = 0b01010101 */
    T_A4SPN_IMM 680,  str_a4spn_imm680    /* nzuimm/4 = 0b10101010 */

    /* --- invariant: C.ADDI4SPN must not modify sp itself, only rd' --- */
    mv      t3, sp
    .option rvc
    c.addi4spn s0, sp, 24
    .option norvc
    la      a0, str_a4spn_sp_preserved
    mv      a1, t3
    mv      a2, sp
    jal     ra, check

    /* --- bit-independence: every individual nzuimm bit (and the max
     *     / alternating-pattern boundary cases) crossed against every
     *     legal rd' register -- see the file-header note above for why
     *     this is necessary and what class of bug it defends against. --- */
    T_A4SPN_CROSS s0, 4   , str_a4spn_x_s0_b2
    T_A4SPN_CROSS s0, 8   , str_a4spn_x_s0_b3
    T_A4SPN_CROSS s0, 16  , str_a4spn_x_s0_b4
    T_A4SPN_CROSS s0, 32  , str_a4spn_x_s0_b5
    T_A4SPN_CROSS s0, 64  , str_a4spn_x_s0_b6
    T_A4SPN_CROSS s0, 128 , str_a4spn_x_s0_b7
    T_A4SPN_CROSS s0, 256 , str_a4spn_x_s0_b8
    T_A4SPN_CROSS s0, 512 , str_a4spn_x_s0_b9
    T_A4SPN_CROSS s0, 1020, str_a4spn_x_s0_max
    T_A4SPN_CROSS s0, 340 , str_a4spn_x_s0_alt0
    T_A4SPN_CROSS s0, 680 , str_a4spn_x_s0_alt1
    T_A4SPN_CROSS s1, 4   , str_a4spn_x_s1_b2
    T_A4SPN_CROSS s1, 8   , str_a4spn_x_s1_b3
    T_A4SPN_CROSS s1, 16  , str_a4spn_x_s1_b4
    T_A4SPN_CROSS s1, 32  , str_a4spn_x_s1_b5
    T_A4SPN_CROSS s1, 64  , str_a4spn_x_s1_b6
    T_A4SPN_CROSS s1, 128 , str_a4spn_x_s1_b7
    T_A4SPN_CROSS s1, 256 , str_a4spn_x_s1_b8
    T_A4SPN_CROSS s1, 512 , str_a4spn_x_s1_b9
    T_A4SPN_CROSS s1, 1020, str_a4spn_x_s1_max
    T_A4SPN_CROSS s1, 340 , str_a4spn_x_s1_alt0
    T_A4SPN_CROSS s1, 680 , str_a4spn_x_s1_alt1
    T_A4SPN_CROSS a0, 4   , str_a4spn_x_a0_b2
    T_A4SPN_CROSS a0, 8   , str_a4spn_x_a0_b3
    T_A4SPN_CROSS a0, 16  , str_a4spn_x_a0_b4
    T_A4SPN_CROSS a0, 32  , str_a4spn_x_a0_b5
    T_A4SPN_CROSS a0, 64  , str_a4spn_x_a0_b6
    T_A4SPN_CROSS a0, 128 , str_a4spn_x_a0_b7
    T_A4SPN_CROSS a0, 256 , str_a4spn_x_a0_b8
    T_A4SPN_CROSS a0, 512 , str_a4spn_x_a0_b9
    T_A4SPN_CROSS a0, 1020, str_a4spn_x_a0_max
    T_A4SPN_CROSS a0, 340 , str_a4spn_x_a0_alt0
    T_A4SPN_CROSS a0, 680 , str_a4spn_x_a0_alt1
    T_A4SPN_CROSS a1, 4   , str_a4spn_x_a1_b2
    T_A4SPN_CROSS a1, 8   , str_a4spn_x_a1_b3
    T_A4SPN_CROSS a1, 16  , str_a4spn_x_a1_b4
    T_A4SPN_CROSS a1, 32  , str_a4spn_x_a1_b5
    T_A4SPN_CROSS a1, 64  , str_a4spn_x_a1_b6
    T_A4SPN_CROSS a1, 128 , str_a4spn_x_a1_b7
    T_A4SPN_CROSS a1, 256 , str_a4spn_x_a1_b8
    T_A4SPN_CROSS a1, 512 , str_a4spn_x_a1_b9
    T_A4SPN_CROSS a1, 1020, str_a4spn_x_a1_max
    T_A4SPN_CROSS a1, 340 , str_a4spn_x_a1_alt0
    T_A4SPN_CROSS a1, 680 , str_a4spn_x_a1_alt1
    T_A4SPN_CROSS a2, 4   , str_a4spn_x_a2_b2
    T_A4SPN_CROSS a2, 8   , str_a4spn_x_a2_b3
    T_A4SPN_CROSS a2, 16  , str_a4spn_x_a2_b4
    T_A4SPN_CROSS a2, 32  , str_a4spn_x_a2_b5
    T_A4SPN_CROSS a2, 64  , str_a4spn_x_a2_b6
    T_A4SPN_CROSS a2, 128 , str_a4spn_x_a2_b7
    T_A4SPN_CROSS a2, 256 , str_a4spn_x_a2_b8
    T_A4SPN_CROSS a2, 512 , str_a4spn_x_a2_b9
    T_A4SPN_CROSS a2, 1020, str_a4spn_x_a2_max
    T_A4SPN_CROSS a2, 340 , str_a4spn_x_a2_alt0
    T_A4SPN_CROSS a2, 680 , str_a4spn_x_a2_alt1
    T_A4SPN_CROSS a3, 4   , str_a4spn_x_a3_b2
    T_A4SPN_CROSS a3, 8   , str_a4spn_x_a3_b3
    T_A4SPN_CROSS a3, 16  , str_a4spn_x_a3_b4
    T_A4SPN_CROSS a3, 32  , str_a4spn_x_a3_b5
    T_A4SPN_CROSS a3, 64  , str_a4spn_x_a3_b6
    T_A4SPN_CROSS a3, 128 , str_a4spn_x_a3_b7
    T_A4SPN_CROSS a3, 256 , str_a4spn_x_a3_b8
    T_A4SPN_CROSS a3, 512 , str_a4spn_x_a3_b9
    T_A4SPN_CROSS a3, 1020, str_a4spn_x_a3_max
    T_A4SPN_CROSS a3, 340 , str_a4spn_x_a3_alt0
    T_A4SPN_CROSS a3, 680 , str_a4spn_x_a3_alt1
    T_A4SPN_CROSS a4, 4   , str_a4spn_x_a4_b2
    T_A4SPN_CROSS a4, 8   , str_a4spn_x_a4_b3
    T_A4SPN_CROSS a4, 16  , str_a4spn_x_a4_b4
    T_A4SPN_CROSS a4, 32  , str_a4spn_x_a4_b5
    T_A4SPN_CROSS a4, 64  , str_a4spn_x_a4_b6
    T_A4SPN_CROSS a4, 128 , str_a4spn_x_a4_b7
    T_A4SPN_CROSS a4, 256 , str_a4spn_x_a4_b8
    T_A4SPN_CROSS a4, 512 , str_a4spn_x_a4_b9
    T_A4SPN_CROSS a4, 1020, str_a4spn_x_a4_max
    T_A4SPN_CROSS a4, 340 , str_a4spn_x_a4_alt0
    T_A4SPN_CROSS a4, 680 , str_a4spn_x_a4_alt1
    T_A4SPN_CROSS a5, 4   , str_a4spn_x_a5_b2
    T_A4SPN_CROSS a5, 8   , str_a4spn_x_a5_b3
    T_A4SPN_CROSS a5, 16  , str_a4spn_x_a5_b4
    T_A4SPN_CROSS a5, 32  , str_a4spn_x_a5_b5
    T_A4SPN_CROSS a5, 64  , str_a4spn_x_a5_b6
    T_A4SPN_CROSS a5, 128 , str_a4spn_x_a5_b7
    T_A4SPN_CROSS a5, 256 , str_a4spn_x_a5_b8
    T_A4SPN_CROSS a5, 512 , str_a4spn_x_a5_b9
    T_A4SPN_CROSS a5, 1020, str_a4spn_x_a5_max
    T_A4SPN_CROSS a5, 340 , str_a4spn_x_a5_alt0
    T_A4SPN_CROSS a5, 680 , str_a4spn_x_a5_alt1

    /* --- within-field pairwise: every pair of individual nzuimm bits
     *     set together, fixed rd'=s0 --- */
    T_A4SPN_PAIR 12  , str_a4spn_pair_b2_b3
    T_A4SPN_PAIR 20  , str_a4spn_pair_b2_b4
    T_A4SPN_PAIR 36  , str_a4spn_pair_b2_b5
    T_A4SPN_PAIR 68  , str_a4spn_pair_b2_b6
    T_A4SPN_PAIR 132 , str_a4spn_pair_b2_b7
    T_A4SPN_PAIR 260 , str_a4spn_pair_b2_b8
    T_A4SPN_PAIR 516 , str_a4spn_pair_b2_b9
    T_A4SPN_PAIR 24  , str_a4spn_pair_b3_b4
    T_A4SPN_PAIR 40  , str_a4spn_pair_b3_b5
    T_A4SPN_PAIR 72  , str_a4spn_pair_b3_b6
    T_A4SPN_PAIR 136 , str_a4spn_pair_b3_b7
    T_A4SPN_PAIR 264 , str_a4spn_pair_b3_b8
    T_A4SPN_PAIR 520 , str_a4spn_pair_b3_b9
    T_A4SPN_PAIR 48  , str_a4spn_pair_b4_b5
    T_A4SPN_PAIR 80  , str_a4spn_pair_b4_b6
    T_A4SPN_PAIR 144 , str_a4spn_pair_b4_b7
    T_A4SPN_PAIR 272 , str_a4spn_pair_b4_b8
    T_A4SPN_PAIR 528 , str_a4spn_pair_b4_b9
    T_A4SPN_PAIR 96  , str_a4spn_pair_b5_b6
    T_A4SPN_PAIR 160 , str_a4spn_pair_b5_b7
    T_A4SPN_PAIR 288 , str_a4spn_pair_b5_b8
    T_A4SPN_PAIR 544 , str_a4spn_pair_b5_b9
    T_A4SPN_PAIR 192 , str_a4spn_pair_b6_b7
    T_A4SPN_PAIR 320 , str_a4spn_pair_b6_b8
    T_A4SPN_PAIR 576 , str_a4spn_pair_b6_b9
    T_A4SPN_PAIR 384 , str_a4spn_pair_b7_b8
    T_A4SPN_PAIR 640 , str_a4spn_pair_b7_b9
    T_A4SPN_PAIR 768 , str_a4spn_pair_b8_b9

    /* --- sp-preserved invariant, re-checked at a spread of different
     *     (rd', nzuimm) combinations from the cross-product above, not
     *     just the single case already covered -- a bit-coupling bug
     *     could plausibly corrupt sp itself for some combinations and
     *     not others. --- */
    mv      t3, sp
    .option rvc
    c.addi4spn a3, sp, 132       /* bits 2+7 */
    .option norvc
    la      a0, str_a4spn_sp_preserved_2
    mv      a1, t3
    mv      a2, sp
    jal     ra, check

    mv      t3, sp
    .option rvc
    c.addi4spn a5, sp, 1020      /* max, all bits */
    .option norvc
    la      a0, str_a4spn_sp_preserved_3
    mv      a1, t3
    mv      a2, sp
    jal     ra, check

    mv      t3, sp
    .option rvc
    c.addi4spn s1, sp, 340       /* alternating pattern */
    .option norvc
    la      a0, str_a4spn_sp_preserved_4
    mv      a1, t3
    mv      a2, sp
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  C.LW : rd' = sign_extend32(*(uint32_t *)(rs1' + uimm))
 *         CL format, rd'/rs1' in x8-x15 only, uimm is an unsigned
 *         multiple of 4 in [0, 124] (5 bits, encoded scrambled as
 *         imm[5:3] | imm[2|6]).
 * ======================================================================= */

/* T_LW_RD rdreg, name -- sweep every legal destination register (fixed
 * base=s0, fixed offset=0, fixed value). Covers rd'==rs1' aliasing when
 * rdreg is s0. Result copied to t2 before a0-a2 are reused, so rdreg
 * may itself be a0/a1/a2. */
.macro T_LW_RD rdreg, name
    la      s0, word_buf
    li      t0, 0x11223344
    sw      t0, 0(s0)
    .option rvc
    c.lw    \rdreg, 0(s0)
    .option norvc
    mv      t2, \rdreg
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, t2
    jal     ra, check
.endm

/* T_LW_RS1 basereg, name -- sweep every legal base register (fixed
 * dest=a4, fixed offset=0, fixed value). Covers rd'==rs1' aliasing when
 * basereg is a4. The base register is consumed by the load itself
 * before a0-a2 are reused, so basereg may itself be a0/a1/a2. */
.macro T_LW_RS1 basereg, name
    la      \basereg, word_buf
    li      t0, 0x11223344
    sw      t0, 0(\basereg)
    .option rvc
    c.lw    a4, 0(\basereg)
    .option norvc
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, a4
    jal     ra, check
.endm

/* T_LW_OFF off, name -- sweep every individual bit (and boundary/mixed
 * values) of the scrambled 5-bit offset field, fixed rd'=a4, rs1'=s0. */
.macro T_LW_OFF off, name
    la      s0, word_buf
    li      t0, 0x11223344
    sw      t0, \off(s0)
    .option rvc
    c.lw    a4, \off(s0)
    .option norvc
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, a4
    jal     ra, check
.endm

/* T_LW_VAL val, expect64, name -- sweep 32-bit data patterns through a
 * fixed rd'=a4/rs1'=s0/offset=0, to verify sign extension is correct
 * (or correctly absent) for each. expect64 is the full 64-bit value
 * the destination register must hold after the sign-extending load. */
.macro T_LW_VAL val, expect64, name
    la      s0, word_buf
    li      t0, \val
    sw      t0, 0(s0)
    .option rvc
    c.lw    a4, 0(s0)
    .option norvc
    la      a0, \name
    li      a1, \expect64
    mv      a2, a4
    jal     ra, check
.endm

test_c_lw:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every legal destination register --- */
    T_LW_RD s0, str_lw_rd_s0
    T_LW_RD s1, str_lw_rd_s1
    T_LW_RD a0, str_lw_rd_a0
    T_LW_RD a1, str_lw_rd_a1
    T_LW_RD a2, str_lw_rd_a2
    T_LW_RD a3, str_lw_rd_a3
    T_LW_RD a4, str_lw_rd_a4
    T_LW_RD a5, str_lw_rd_a5

    /* --- every legal base register (rs1'==rd' aliasing at a4) --- */
    T_LW_RS1 s0, str_lw_rs1_s0
    T_LW_RS1 s1, str_lw_rs1_s1
    T_LW_RS1 a0, str_lw_rs1_a0
    T_LW_RS1 a1, str_lw_rs1_a1
    T_LW_RS1 a2, str_lw_rs1_a2
    T_LW_RS1 a3, str_lw_rs1_a3
    T_LW_RS1 a4, str_lw_rs1_a4
    T_LW_RS1 a5, str_lw_rs1_a5

    /* --- every individual offset-field bit, boundaries, mixed patterns --- */
    T_LW_OFF 0,   str_lw_off0        /* zero offset (legal boundary) */
    T_LW_OFF 4,   str_lw_off4        /* bit 2 */
    T_LW_OFF 8,   str_lw_off8        /* bit 3 */
    T_LW_OFF 16,  str_lw_off16       /* bit 4 */
    T_LW_OFF 32,  str_lw_off32       /* bit 5 */
    T_LW_OFF 64,  str_lw_off64       /* bit 6 (max single bit) */
    T_LW_OFF 124, str_lw_off124      /* max offset, all 5 bits set */
    T_LW_OFF 84,  str_lw_off84       /* uimm/4 = 0b10101 */
    T_LW_OFF 40,  str_lw_off40       /* uimm/4 = 0b01010 */

    /* --- data patterns: verify sign extension is correct in every case --- */
    T_LW_VAL 0x00000000, 0x0000000000000000, str_lw_val_zero
    T_LW_VAL 0xffffffff, 0xffffffffffffffff, str_lw_val_allones
    T_LW_VAL 0x7fffffff, 0x000000007fffffff, str_lw_val_maxpos
    T_LW_VAL 0x80000000, 0xffffffff80000000, str_lw_val_minneg
    T_LW_VAL 0xaaaaaaaa, 0xffffffffaaaaaaaa, str_lw_val_alt1
    T_LW_VAL 0x55555555, 0x0000000055555555, str_lw_val_alt0
    T_LW_VAL 0xdeadbeef, 0xffffffffdeadbeef, str_lw_val_deadbeef
    T_LW_VAL 0x00000001, 0x0000000000000001, str_lw_val_one

    /* --- invariants: C.LW must not modify rs1' or memory (pure read) --- */
    la      s0, word_buf
    li      t0, 0xcafed00d
    sw      t0, 0(s0)
    mv      t3, s0
    .option rvc
    c.lw    a4, 0(s0)
    .option norvc
    la      a0, str_lw_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    lwu     t4, 0(s0)               /* lwu (zero-extend) so the check isn't
                                      * confused by sign-extension noise --
                                      * 0xcafed00d has bit 31 set */
    la      a0, str_lw_mem_unchanged
    li      a1, 0xcafed00d
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  C.SW : *(uint32_t *)(rs1' + uimm) = rs2'[31:0]
 *         CS format, rd'/rs1' in x8-x15 only, uimm is an unsigned
 *         multiple of 4 in [0, 124] (same scrambled 5-bit field as
 *         C.LW). Unlike a load, a store never writes back to either
 *         operand register, so rs1'==rs2' aliasing has no hazard --
 *         it just means both roles read the same single value, so the
 *         data written equals whatever that shared register holds.
 * ======================================================================= */

/* T_SW_RS2 srcreg, name -- sweep every legal source register except s0
 * (base is fixed at s0; rs1'==rs2'==s0 is covered separately by
 * T_SW_ALIAS since it needs different expected-value logic). srcreg
 * may be a0/a1/a2 -- memory is read back into t2 before a0-a2 are
 * reused. */
.macro T_SW_RS2 srcreg, name
    la      s0, word_buf
    li      \srcreg, 0x11223344
    .option rvc
    c.sw    \srcreg, 0(s0)
    .option norvc
    lwu     t2, 0(s0)
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, t2
    jal     ra, check
.endm

/* T_SW_RS1 basereg, name -- sweep every legal base register except a4
 * (source is fixed at a4; rs1'==rs2'==a4 covered by T_SW_ALIAS).
 * basereg may be a0/a1/a2 -- consumed as the read-back address before
 * a0-a2 are reused. */
.macro T_SW_RS1 basereg, name
    la      \basereg, word_buf
    li      a4, 0x11223344
    .option rvc
    c.sw    a4, 0(\basereg)
    .option norvc
    lwu     t2, 0(\basereg)
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, t2
    jal     ra, check
.endm

/* T_SW_ALIAS reg, name -- rs1'==rs2' aliasing: the single register is
 * simultaneously the base address and the stored data, so the
 * expected memory content is just that register's own low 32 bits
 * (the address itself). Used once at s0 (fills the rs2'=s0 gap left
 * by T_SW_RS2's sweep) and once at a4 (fills the rs1'=a4 gap left by
 * T_SW_RS1's sweep), giving full 8-register coverage of both fields. */
.macro T_SW_ALIAS reg, name
    la      \reg, word_buf
    mv      t1, \reg
    .option rvc
    c.sw    \reg, 0(\reg)
    .option norvc
    lwu     t2, 0(\reg)
    li      t4, 0xffffffff
    and     a1, t1, t4
    mv      a2, t2
    la      a0, \name
    jal     ra, check
.endm

/* T_SW_OFF off, name -- sweep every individual bit (and boundary/mixed
 * values) of the scrambled 5-bit offset field, fixed rs1'=s0, rs2'=a4. */
.macro T_SW_OFF off, name
    la      s0, word_buf
    li      a4, 0x11223344
    .option rvc
    c.sw    a4, \off(s0)
    .option norvc
    lwu     t2, \off(s0)
    la      a0, \name
    li      a1, 0x11223344
    mv      a2, t2
    jal     ra, check
.endm

/* T_SW_VAL val, name -- sweep 32-bit data patterns through a fixed
 * rs1'=s0/rs2'=a4/offset=0. */
.macro T_SW_VAL val, name
    la      s0, word_buf
    li      a4, \val
    .option rvc
    c.sw    a4, 0(s0)
    .option norvc
    lwu     t2, 0(s0)
    la      a0, \name
    li      a1, \val
    mv      a2, t2
    jal     ra, check
.endm

test_c_sw:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every legal source register (except s0, see T_SW_ALIAS) --- */
    T_SW_RS2 s1, str_sw_rs2_s1
    T_SW_RS2 a0, str_sw_rs2_a0
    T_SW_RS2 a1, str_sw_rs2_a1
    T_SW_RS2 a2, str_sw_rs2_a2
    T_SW_RS2 a3, str_sw_rs2_a3
    T_SW_RS2 a4, str_sw_rs2_a4
    T_SW_RS2 a5, str_sw_rs2_a5

    /* --- every legal base register (except a4, see T_SW_ALIAS) --- */
    T_SW_RS1 s0, str_sw_rs1_s0
    T_SW_RS1 s1, str_sw_rs1_s1
    T_SW_RS1 a0, str_sw_rs1_a0
    T_SW_RS1 a1, str_sw_rs1_a1
    T_SW_RS1 a2, str_sw_rs1_a2
    T_SW_RS1 a3, str_sw_rs1_a3
    T_SW_RS1 a5, str_sw_rs1_a5

    /* --- rs1'==rs2' aliasing, at both registers needed for full
     *     coverage of each field (see macro comment above) --- */
    T_SW_ALIAS s0, str_sw_alias_s0
    T_SW_ALIAS a4, str_sw_alias_a4

    /* --- every individual offset-field bit, boundaries, mixed patterns --- */
    T_SW_OFF 0,   str_sw_off0        /* zero offset (legal boundary) */
    T_SW_OFF 4,   str_sw_off4        /* bit 2 */
    T_SW_OFF 8,   str_sw_off8        /* bit 3 */
    T_SW_OFF 16,  str_sw_off16       /* bit 4 */
    T_SW_OFF 32,  str_sw_off32       /* bit 5 */
    T_SW_OFF 64,  str_sw_off64       /* bit 6 (max single bit) */
    T_SW_OFF 124, str_sw_off124      /* max offset, all 5 bits set */
    T_SW_OFF 84,  str_sw_off84       /* uimm/4 = 0b10101 */
    T_SW_OFF 40,  str_sw_off40       /* uimm/4 = 0b01010 */

    /* --- data patterns --- */
    T_SW_VAL 0x00000000, str_sw_val_zero
    T_SW_VAL 0xffffffff, str_sw_val_allones
    T_SW_VAL 0x7fffffff, str_sw_val_maxpos
    T_SW_VAL 0x80000000, str_sw_val_minneg
    T_SW_VAL 0xaaaaaaaa, str_sw_val_alt1
    T_SW_VAL 0x55555555, str_sw_val_alt0
    T_SW_VAL 0xdeadbeef, str_sw_val_deadbeef
    T_SW_VAL 0x00000001, str_sw_val_one

    /* --- edge case: source register has non-zero upper 32 bits; only
     *     the low 32 bits may be written to memory --- */
    la      s0, word_buf
    li      a4, 0xdeadbeef12345678
    .option rvc
    c.sw    a4, 0(s0)
    .option norvc
    lwu     t2, 0(s0)
    la      a0, str_sw_upper_bits_ignored
    li      a1, 0x12345678
    mv      a2, t2
    jal     ra, check

    /* --- invariant: C.SW touches exactly 4 bytes, nothing adjacent --- */
    la      s0, word_buf
    li      t0, 0xaaaaaaaa
    sw      t0, 60(s0)              /* sentinel word before target */
    li      t0, 0xbbbbbbbb
    sw      t0, 68(s0)              /* sentinel word after target */
    li      a4, 0x11223344
    .option rvc
    c.sw    a4, 64(s0)
    .option norvc

    lwu     t1, 60(s0)
    la      a0, str_sw_adjacent_before
    li      a1, 0xaaaaaaaa
    mv      a2, t1
    jal     ra, check

    lwu     t1, 68(s0)
    la      a0, str_sw_adjacent_after
    li      a1, 0xbbbbbbbb
    mv      a2, t1
    jal     ra, check

    lwu     t1, 64(s0)
    la      a0, str_sw_target_correct
    li      a1, 0x11223344
    mv      a2, t1
    jal     ra, check

    /* --- invariants: C.SW must not modify rs1' or rs2' (pure write) --- */
    la      s0, word_buf
    li      a4, 0xcafed00d
    mv      t1, s0
    mv      t2, a4
    .option rvc
    c.sw    a4, 0(s0)
    .option norvc
    la      a0, str_sw_rs1_preserved
    mv      a1, t1
    mv      a2, s0
    jal     ra, check

    la      a0, str_sw_rs2_preserved
    mv      a1, t2
    mv      a2, a4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  C.SD : *(uint64_t *)(rs1' + uimm) = rs2'  (CS format, rs1'/rs2' in
 *         x8-x15 only, uimm is an unsigned multiple of 8 in [0, 248],
 *         5 bits encoded scrambled as imm[5:3] | imm[7:6]). Same
 *         "store never writes back" reasoning as C.SW applies to
 *         rs1'==rs2' aliasing here, except the full 64 bits (not just
 *         the low 32) are both the address source and the data.
 * ======================================================================= */

.macro T_SD_RS2 srcreg, name
    la      s0, dword_buf
    li      \srcreg, 0x1122334455667788
    .option rvc
    c.sd    \srcreg, 0(s0)
    .option norvc
    ld      t2, 0(s0)
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_RS1 basereg, name
    la      \basereg, dword_buf
    li      a4, 0x1122334455667788
    .option rvc
    c.sd    a4, 0(\basereg)
    .option norvc
    ld      t2, 0(\basereg)
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, t2
    jal     ra, check
.endm

/* rs1'==rs2' aliasing: unlike C.SW, no masking is needed for the
 * expected value -- the whole 64-bit register is both the address
 * source and the stored data, so the memory content should just equal
 * the register's own (address) value, in full. */
.macro T_SD_ALIAS reg, name
    la      \reg, dword_buf
    mv      t1, \reg
    .option rvc
    c.sd    \reg, 0(\reg)
    .option norvc
    ld      t2, 0(\reg)
    la      a0, \name
    mv      a1, t1
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_OFF off, name
    la      s0, dword_buf
    li      a4, 0x1122334455667788
    .option rvc
    c.sd    a4, \off(s0)
    .option norvc
    ld      t2, \off(s0)
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, t2
    jal     ra, check
.endm

.macro T_SD_VAL val, name
    la      s0, dword_buf
    li      a4, \val
    .option rvc
    c.sd    a4, 0(s0)
    .option norvc
    ld      t2, 0(s0)
    la      a0, \name
    li      a1, \val
    mv      a2, t2
    jal     ra, check
.endm

test_c_sd:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every legal source register (except s0, see T_SD_ALIAS) --- */
    T_SD_RS2 s1, str_sd_rs2_s1
    T_SD_RS2 a0, str_sd_rs2_a0
    T_SD_RS2 a1, str_sd_rs2_a1
    T_SD_RS2 a2, str_sd_rs2_a2
    T_SD_RS2 a3, str_sd_rs2_a3
    T_SD_RS2 a4, str_sd_rs2_a4
    T_SD_RS2 a5, str_sd_rs2_a5

    /* --- every legal base register (except a4, see T_SD_ALIAS) --- */
    T_SD_RS1 s0, str_sd_rs1_s0
    T_SD_RS1 s1, str_sd_rs1_s1
    T_SD_RS1 a0, str_sd_rs1_a0
    T_SD_RS1 a1, str_sd_rs1_a1
    T_SD_RS1 a2, str_sd_rs1_a2
    T_SD_RS1 a3, str_sd_rs1_a3
    T_SD_RS1 a5, str_sd_rs1_a5

    /* --- rs1'==rs2' aliasing, at both registers needed for full
     *     coverage of each field --- */
    T_SD_ALIAS s0, str_sd_alias_s0
    T_SD_ALIAS a4, str_sd_alias_a4

    /* --- every individual offset-field bit, boundaries, mixed patterns --- */
    T_SD_OFF 0,   str_sd_off0        /* zero offset (legal boundary) */
    T_SD_OFF 8,   str_sd_off8        /* bit 3 */
    T_SD_OFF 16,  str_sd_off16       /* bit 4 */
    T_SD_OFF 32,  str_sd_off32       /* bit 5 */
    T_SD_OFF 64,  str_sd_off64       /* bit 6 */
    T_SD_OFF 128, str_sd_off128      /* bit 7 (max single bit) */
    T_SD_OFF 248, str_sd_off248      /* max offset, all 5 bits set */
    T_SD_OFF 168, str_sd_off168      /* uimm/8 = 0b10101 */
    T_SD_OFF 80,  str_sd_off80       /* uimm/8 = 0b01010 */

    /* --- data patterns: also implicitly proves all 64 bits (not just
     *     the low 32) really got written, since upper/lower halves
     *     differ in most of these --- */
    T_SD_VAL 0x0000000000000000, str_sd_val_zero
    T_SD_VAL 0xffffffffffffffff, str_sd_val_allones
    T_SD_VAL 0x7fffffffffffffff, str_sd_val_maxpos
    T_SD_VAL 0x8000000000000000, str_sd_val_minneg
    T_SD_VAL 0xaaaaaaaaaaaaaaaa, str_sd_val_alt1
    T_SD_VAL 0x5555555555555555, str_sd_val_alt0
    T_SD_VAL 0x0123456789abcdef, str_sd_val_walking
    T_SD_VAL 0x0000000000000001, str_sd_val_one

    /* --- invariant: C.SD touches exactly 8 bytes, nothing adjacent --- */
    la      s0, dword_buf
    li      t0, 0xaaaaaaaaaaaaaaaa
    sd      t0, 120(s0)             /* sentinel doubleword before target */
    li      t0, 0xbbbbbbbbbbbbbbbb
    sd      t0, 136(s0)             /* sentinel doubleword after target */
    li      a4, 0x1122334455667788
    .option rvc
    c.sd    a4, 128(s0)
    .option norvc

    ld      t1, 120(s0)
    la      a0, str_sd_adjacent_before
    li      a1, 0xaaaaaaaaaaaaaaaa
    mv      a2, t1
    jal     ra, check

    ld      t1, 136(s0)
    la      a0, str_sd_adjacent_after
    li      a1, 0xbbbbbbbbbbbbbbbb
    mv      a2, t1
    jal     ra, check

    ld      t1, 128(s0)
    la      a0, str_sd_target_correct
    li      a1, 0x1122334455667788
    mv      a2, t1
    jal     ra, check

    /* --- invariants: C.SD must not modify rs1' or rs2' (pure write) --- */
    la      s0, dword_buf
    li      a4, 0xcafebabedeadbeef
    mv      t1, s0
    mv      t2, a4
    .option rvc
    c.sd    a4, 0(s0)
    .option norvc
    la      a0, str_sd_rs1_preserved
    mv      a1, t1
    mv      a2, s0
    jal     ra, check

    la      a0, str_sd_rs2_preserved
    mv      a1, t2
    mv      a2, a4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  C.LD : rd' = *(uint64_t *)(rs1' + uimm)  (CL format, rd'/rs1' in
 *         x8-x15 only, same scrambled 5-bit/multiple-of-8 offset field
 *         as C.SD). No sign-extension question here (unlike C.LW) --
 *         the full 64 bits are loaded exactly.
 * ======================================================================= */

.macro T_LD_RD rdreg, name
    la      s0, dword_buf
    li      t0, 0x1122334455667788
    sd      t0, 0(s0)
    .option rvc
    c.ld    \rdreg, 0(s0)
    .option norvc
    mv      t2, \rdreg
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, t2
    jal     ra, check
.endm

.macro T_LD_RS1 basereg, name
    la      \basereg, dword_buf
    li      t0, 0x1122334455667788
    sd      t0, 0(\basereg)
    .option rvc
    c.ld    a4, 0(\basereg)
    .option norvc
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LD_OFF off, name
    la      s0, dword_buf
    li      t0, 0x1122334455667788
    sd      t0, \off(s0)
    .option rvc
    c.ld    a4, \off(s0)
    .option norvc
    la      a0, \name
    li      a1, 0x1122334455667788
    mv      a2, a4
    jal     ra, check
.endm

.macro T_LD_VAL val, name
    la      s0, dword_buf
    li      t0, \val
    sd      t0, 0(s0)
    .option rvc
    c.ld    a4, 0(s0)
    .option norvc
    la      a0, \name
    li      a1, \val
    mv      a2, a4
    jal     ra, check
.endm

test_c_ld:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    /* --- every legal destination register --- */
    T_LD_RD s0, str_ld_rd_s0
    T_LD_RD s1, str_ld_rd_s1
    T_LD_RD a0, str_ld_rd_a0
    T_LD_RD a1, str_ld_rd_a1
    T_LD_RD a2, str_ld_rd_a2
    T_LD_RD a3, str_ld_rd_a3
    T_LD_RD a4, str_ld_rd_a4
    T_LD_RD a5, str_ld_rd_a5

    /* --- every legal base register (rs1'==rd' aliasing at a4) --- */
    T_LD_RS1 s0, str_ld_rs1_s0
    T_LD_RS1 s1, str_ld_rs1_s1
    T_LD_RS1 a0, str_ld_rs1_a0
    T_LD_RS1 a1, str_ld_rs1_a1
    T_LD_RS1 a2, str_ld_rs1_a2
    T_LD_RS1 a3, str_ld_rs1_a3
    T_LD_RS1 a4, str_ld_rs1_a4
    T_LD_RS1 a5, str_ld_rs1_a5

    /* --- every individual offset-field bit, boundaries, mixed patterns --- */
    T_LD_OFF 0,   str_ld_off0        /* zero offset (legal boundary) */
    T_LD_OFF 8,   str_ld_off8        /* bit 3 */
    T_LD_OFF 16,  str_ld_off16       /* bit 4 */
    T_LD_OFF 32,  str_ld_off32       /* bit 5 */
    T_LD_OFF 64,  str_ld_off64       /* bit 6 */
    T_LD_OFF 128, str_ld_off128      /* bit 7 (max single bit) */
    T_LD_OFF 248, str_ld_off248      /* max offset, all 5 bits set */
    T_LD_OFF 168, str_ld_off168      /* uimm/8 = 0b10101 */
    T_LD_OFF 80,  str_ld_off80       /* uimm/8 = 0b01010 */

    /* --- data patterns --- */
    T_LD_VAL 0x0000000000000000, str_ld_val_zero
    T_LD_VAL 0xffffffffffffffff, str_ld_val_allones
    T_LD_VAL 0x7fffffffffffffff, str_ld_val_maxpos
    T_LD_VAL 0x8000000000000000, str_ld_val_minneg
    T_LD_VAL 0xaaaaaaaaaaaaaaaa, str_ld_val_alt1
    T_LD_VAL 0x5555555555555555, str_ld_val_alt0
    T_LD_VAL 0x0123456789abcdef, str_ld_val_walking
    T_LD_VAL 0x0000000000000001, str_ld_val_one

    /* --- invariants: C.LD must not modify rs1' or memory (pure read) --- */
    la      s0, dword_buf
    li      t0, 0xcafebabedeadbeef
    sd      t0, 0(s0)
    mv      t3, s0
    .option rvc
    c.ld    a4, 0(s0)
    .option norvc
    la      a0, str_ld_rs1_preserved
    mv      a1, t3
    mv      a2, s0
    jal     ra, check

    ld      t4, 0(s0)
    la      a0, str_ld_mem_unchanged
    li      a1, 0xcafebabedeadbeef
    mv      a2, t4
    jal     ra, check

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  QUADRANT 0 : C.ADDI4SPN, C.LW, C.LD, C.SW, C.SD
 * ======================================================================= */
tests_quadrant0:
    addi    sp, sp, -16
    sd      ra, 0(sp)

    jal     ra, test_c_addi4spn

    jal     ra, test_c_sw

    jal     ra, test_c_lw

    jal     ra, test_c_sd

    jal     ra, test_c_ld

    ld      ra, 0(sp)
    addi    sp, sp, 16
    ret

/* =======================================================================
 *  Strings
 * ======================================================================= */
    .section .rodata
str_a4spn_reg_s0:    .asciz "C.ADDI4SPN rd=s0(x8)"
str_a4spn_reg_s1:    .asciz "C.ADDI4SPN rd=s1(x9)"
str_a4spn_reg_a0:    .asciz "C.ADDI4SPN rd=a0(x10)"
str_a4spn_reg_a1:    .asciz "C.ADDI4SPN rd=a1(x11)"
str_a4spn_reg_a2:    .asciz "C.ADDI4SPN rd=a2(x12)"
str_a4spn_reg_a3:    .asciz "C.ADDI4SPN rd=a3(x13)"
str_a4spn_reg_a4:    .asciz "C.ADDI4SPN rd=a4(x14)"
str_a4spn_reg_a5:    .asciz "C.ADDI4SPN rd=a5(x15)"
str_a4spn_imm4:      .asciz "C.ADDI4SPN imm=4 (bit2)"
str_a4spn_imm8:      .asciz "C.ADDI4SPN imm=8 (bit3)"
str_a4spn_imm16:     .asciz "C.ADDI4SPN imm=16 (bit4)"
str_a4spn_imm32:     .asciz "C.ADDI4SPN imm=32 (bit5)"
str_a4spn_imm64:     .asciz "C.ADDI4SPN imm=64 (bit6)"
str_a4spn_imm128:    .asciz "C.ADDI4SPN imm=128 (bit7)"
str_a4spn_imm256:    .asciz "C.ADDI4SPN imm=256 (bit8)"
str_a4spn_imm512:    .asciz "C.ADDI4SPN imm=512 (bit9, max single bit)"
str_a4spn_imm1020:   .asciz "C.ADDI4SPN imm=1020 (max, all bits)"
str_a4spn_imm340:    .asciz "C.ADDI4SPN imm=340 (alt pattern 01010101)"
str_a4spn_imm680:    .asciz "C.ADDI4SPN imm=680 (alt pattern 10101010)"
str_a4spn_sp_preserved: .asciz "C.ADDI4SPN sp unmodified"
str_a4spn_x_s0_b2: .asciz "C.ADDI4SPN rd=s0 x nzuimm=4 (bit2)"
str_a4spn_x_s0_b3: .asciz "C.ADDI4SPN rd=s0 x nzuimm=8 (bit3)"
str_a4spn_x_s0_b4: .asciz "C.ADDI4SPN rd=s0 x nzuimm=16 (bit4)"
str_a4spn_x_s0_b5: .asciz "C.ADDI4SPN rd=s0 x nzuimm=32 (bit5)"
str_a4spn_x_s0_b6: .asciz "C.ADDI4SPN rd=s0 x nzuimm=64 (bit6)"
str_a4spn_x_s0_b7: .asciz "C.ADDI4SPN rd=s0 x nzuimm=128 (bit7)"
str_a4spn_x_s0_b8: .asciz "C.ADDI4SPN rd=s0 x nzuimm=256 (bit8)"
str_a4spn_x_s0_b9: .asciz "C.ADDI4SPN rd=s0 x nzuimm=512 (bit9)"
str_a4spn_x_s0_max: .asciz "C.ADDI4SPN rd=s0 x nzuimm=1020 (max)"
str_a4spn_x_s0_alt0: .asciz "C.ADDI4SPN rd=s0 x nzuimm=340 (alt0)"
str_a4spn_x_s0_alt1: .asciz "C.ADDI4SPN rd=s0 x nzuimm=680 (alt1)"
str_a4spn_x_s1_b2: .asciz "C.ADDI4SPN rd=s1 x nzuimm=4 (bit2)"
str_a4spn_x_s1_b3: .asciz "C.ADDI4SPN rd=s1 x nzuimm=8 (bit3)"
str_a4spn_x_s1_b4: .asciz "C.ADDI4SPN rd=s1 x nzuimm=16 (bit4)"
str_a4spn_x_s1_b5: .asciz "C.ADDI4SPN rd=s1 x nzuimm=32 (bit5)"
str_a4spn_x_s1_b6: .asciz "C.ADDI4SPN rd=s1 x nzuimm=64 (bit6)"
str_a4spn_x_s1_b7: .asciz "C.ADDI4SPN rd=s1 x nzuimm=128 (bit7)"
str_a4spn_x_s1_b8: .asciz "C.ADDI4SPN rd=s1 x nzuimm=256 (bit8)"
str_a4spn_x_s1_b9: .asciz "C.ADDI4SPN rd=s1 x nzuimm=512 (bit9)"
str_a4spn_x_s1_max: .asciz "C.ADDI4SPN rd=s1 x nzuimm=1020 (max)"
str_a4spn_x_s1_alt0: .asciz "C.ADDI4SPN rd=s1 x nzuimm=340 (alt0)"
str_a4spn_x_s1_alt1: .asciz "C.ADDI4SPN rd=s1 x nzuimm=680 (alt1)"
str_a4spn_x_a0_b2: .asciz "C.ADDI4SPN rd=a0 x nzuimm=4 (bit2)"
str_a4spn_x_a0_b3: .asciz "C.ADDI4SPN rd=a0 x nzuimm=8 (bit3)"
str_a4spn_x_a0_b4: .asciz "C.ADDI4SPN rd=a0 x nzuimm=16 (bit4)"
str_a4spn_x_a0_b5: .asciz "C.ADDI4SPN rd=a0 x nzuimm=32 (bit5)"
str_a4spn_x_a0_b6: .asciz "C.ADDI4SPN rd=a0 x nzuimm=64 (bit6)"
str_a4spn_x_a0_b7: .asciz "C.ADDI4SPN rd=a0 x nzuimm=128 (bit7)"
str_a4spn_x_a0_b8: .asciz "C.ADDI4SPN rd=a0 x nzuimm=256 (bit8)"
str_a4spn_x_a0_b9: .asciz "C.ADDI4SPN rd=a0 x nzuimm=512 (bit9)"
str_a4spn_x_a0_max: .asciz "C.ADDI4SPN rd=a0 x nzuimm=1020 (max)"
str_a4spn_x_a0_alt0: .asciz "C.ADDI4SPN rd=a0 x nzuimm=340 (alt0)"
str_a4spn_x_a0_alt1: .asciz "C.ADDI4SPN rd=a0 x nzuimm=680 (alt1)"
str_a4spn_x_a1_b2: .asciz "C.ADDI4SPN rd=a1 x nzuimm=4 (bit2)"
str_a4spn_x_a1_b3: .asciz "C.ADDI4SPN rd=a1 x nzuimm=8 (bit3)"
str_a4spn_x_a1_b4: .asciz "C.ADDI4SPN rd=a1 x nzuimm=16 (bit4)"
str_a4spn_x_a1_b5: .asciz "C.ADDI4SPN rd=a1 x nzuimm=32 (bit5)"
str_a4spn_x_a1_b6: .asciz "C.ADDI4SPN rd=a1 x nzuimm=64 (bit6)"
str_a4spn_x_a1_b7: .asciz "C.ADDI4SPN rd=a1 x nzuimm=128 (bit7)"
str_a4spn_x_a1_b8: .asciz "C.ADDI4SPN rd=a1 x nzuimm=256 (bit8)"
str_a4spn_x_a1_b9: .asciz "C.ADDI4SPN rd=a1 x nzuimm=512 (bit9)"
str_a4spn_x_a1_max: .asciz "C.ADDI4SPN rd=a1 x nzuimm=1020 (max)"
str_a4spn_x_a1_alt0: .asciz "C.ADDI4SPN rd=a1 x nzuimm=340 (alt0)"
str_a4spn_x_a1_alt1: .asciz "C.ADDI4SPN rd=a1 x nzuimm=680 (alt1)"
str_a4spn_x_a2_b2: .asciz "C.ADDI4SPN rd=a2 x nzuimm=4 (bit2)"
str_a4spn_x_a2_b3: .asciz "C.ADDI4SPN rd=a2 x nzuimm=8 (bit3)"
str_a4spn_x_a2_b4: .asciz "C.ADDI4SPN rd=a2 x nzuimm=16 (bit4)"
str_a4spn_x_a2_b5: .asciz "C.ADDI4SPN rd=a2 x nzuimm=32 (bit5)"
str_a4spn_x_a2_b6: .asciz "C.ADDI4SPN rd=a2 x nzuimm=64 (bit6)"
str_a4spn_x_a2_b7: .asciz "C.ADDI4SPN rd=a2 x nzuimm=128 (bit7)"
str_a4spn_x_a2_b8: .asciz "C.ADDI4SPN rd=a2 x nzuimm=256 (bit8)"
str_a4spn_x_a2_b9: .asciz "C.ADDI4SPN rd=a2 x nzuimm=512 (bit9)"
str_a4spn_x_a2_max: .asciz "C.ADDI4SPN rd=a2 x nzuimm=1020 (max)"
str_a4spn_x_a2_alt0: .asciz "C.ADDI4SPN rd=a2 x nzuimm=340 (alt0)"
str_a4spn_x_a2_alt1: .asciz "C.ADDI4SPN rd=a2 x nzuimm=680 (alt1)"
str_a4spn_x_a3_b2: .asciz "C.ADDI4SPN rd=a3 x nzuimm=4 (bit2)"
str_a4spn_x_a3_b3: .asciz "C.ADDI4SPN rd=a3 x nzuimm=8 (bit3)"
str_a4spn_x_a3_b4: .asciz "C.ADDI4SPN rd=a3 x nzuimm=16 (bit4)"
str_a4spn_x_a3_b5: .asciz "C.ADDI4SPN rd=a3 x nzuimm=32 (bit5)"
str_a4spn_x_a3_b6: .asciz "C.ADDI4SPN rd=a3 x nzuimm=64 (bit6)"
str_a4spn_x_a3_b7: .asciz "C.ADDI4SPN rd=a3 x nzuimm=128 (bit7)"
str_a4spn_x_a3_b8: .asciz "C.ADDI4SPN rd=a3 x nzuimm=256 (bit8)"
str_a4spn_x_a3_b9: .asciz "C.ADDI4SPN rd=a3 x nzuimm=512 (bit9)"
str_a4spn_x_a3_max: .asciz "C.ADDI4SPN rd=a3 x nzuimm=1020 (max)"
str_a4spn_x_a3_alt0: .asciz "C.ADDI4SPN rd=a3 x nzuimm=340 (alt0)"
str_a4spn_x_a3_alt1: .asciz "C.ADDI4SPN rd=a3 x nzuimm=680 (alt1)"
str_a4spn_x_a4_b2: .asciz "C.ADDI4SPN rd=a4 x nzuimm=4 (bit2)"
str_a4spn_x_a4_b3: .asciz "C.ADDI4SPN rd=a4 x nzuimm=8 (bit3)"
str_a4spn_x_a4_b4: .asciz "C.ADDI4SPN rd=a4 x nzuimm=16 (bit4)"
str_a4spn_x_a4_b5: .asciz "C.ADDI4SPN rd=a4 x nzuimm=32 (bit5)"
str_a4spn_x_a4_b6: .asciz "C.ADDI4SPN rd=a4 x nzuimm=64 (bit6)"
str_a4spn_x_a4_b7: .asciz "C.ADDI4SPN rd=a4 x nzuimm=128 (bit7)"
str_a4spn_x_a4_b8: .asciz "C.ADDI4SPN rd=a4 x nzuimm=256 (bit8)"
str_a4spn_x_a4_b9: .asciz "C.ADDI4SPN rd=a4 x nzuimm=512 (bit9)"
str_a4spn_x_a4_max: .asciz "C.ADDI4SPN rd=a4 x nzuimm=1020 (max)"
str_a4spn_x_a4_alt0: .asciz "C.ADDI4SPN rd=a4 x nzuimm=340 (alt0)"
str_a4spn_x_a4_alt1: .asciz "C.ADDI4SPN rd=a4 x nzuimm=680 (alt1)"
str_a4spn_x_a5_b2: .asciz "C.ADDI4SPN rd=a5 x nzuimm=4 (bit2)"
str_a4spn_x_a5_b3: .asciz "C.ADDI4SPN rd=a5 x nzuimm=8 (bit3)"
str_a4spn_x_a5_b4: .asciz "C.ADDI4SPN rd=a5 x nzuimm=16 (bit4)"
str_a4spn_x_a5_b5: .asciz "C.ADDI4SPN rd=a5 x nzuimm=32 (bit5)"
str_a4spn_x_a5_b6: .asciz "C.ADDI4SPN rd=a5 x nzuimm=64 (bit6)"
str_a4spn_x_a5_b7: .asciz "C.ADDI4SPN rd=a5 x nzuimm=128 (bit7)"
str_a4spn_x_a5_b8: .asciz "C.ADDI4SPN rd=a5 x nzuimm=256 (bit8)"
str_a4spn_x_a5_b9: .asciz "C.ADDI4SPN rd=a5 x nzuimm=512 (bit9)"
str_a4spn_x_a5_max: .asciz "C.ADDI4SPN rd=a5 x nzuimm=1020 (max)"
str_a4spn_x_a5_alt0: .asciz "C.ADDI4SPN rd=a5 x nzuimm=340 (alt0)"
str_a4spn_x_a5_alt1: .asciz "C.ADDI4SPN rd=a5 x nzuimm=680 (alt1)"
str_a4spn_pair_b2_b3: .asciz "C.ADDI4SPN nzuimm bits 2+3 together (=12)"
str_a4spn_pair_b2_b4: .asciz "C.ADDI4SPN nzuimm bits 2+4 together (=20)"
str_a4spn_pair_b2_b5: .asciz "C.ADDI4SPN nzuimm bits 2+5 together (=36)"
str_a4spn_pair_b2_b6: .asciz "C.ADDI4SPN nzuimm bits 2+6 together (=68)"
str_a4spn_pair_b2_b7: .asciz "C.ADDI4SPN nzuimm bits 2+7 together (=132)"
str_a4spn_pair_b2_b8: .asciz "C.ADDI4SPN nzuimm bits 2+8 together (=260)"
str_a4spn_pair_b2_b9: .asciz "C.ADDI4SPN nzuimm bits 2+9 together (=516)"
str_a4spn_pair_b3_b4: .asciz "C.ADDI4SPN nzuimm bits 3+4 together (=24)"
str_a4spn_pair_b3_b5: .asciz "C.ADDI4SPN nzuimm bits 3+5 together (=40)"
str_a4spn_pair_b3_b6: .asciz "C.ADDI4SPN nzuimm bits 3+6 together (=72)"
str_a4spn_pair_b3_b7: .asciz "C.ADDI4SPN nzuimm bits 3+7 together (=136)"
str_a4spn_pair_b3_b8: .asciz "C.ADDI4SPN nzuimm bits 3+8 together (=264)"
str_a4spn_pair_b3_b9: .asciz "C.ADDI4SPN nzuimm bits 3+9 together (=520)"
str_a4spn_pair_b4_b5: .asciz "C.ADDI4SPN nzuimm bits 4+5 together (=48)"
str_a4spn_pair_b4_b6: .asciz "C.ADDI4SPN nzuimm bits 4+6 together (=80)"
str_a4spn_pair_b4_b7: .asciz "C.ADDI4SPN nzuimm bits 4+7 together (=144)"
str_a4spn_pair_b4_b8: .asciz "C.ADDI4SPN nzuimm bits 4+8 together (=272)"
str_a4spn_pair_b4_b9: .asciz "C.ADDI4SPN nzuimm bits 4+9 together (=528)"
str_a4spn_pair_b5_b6: .asciz "C.ADDI4SPN nzuimm bits 5+6 together (=96)"
str_a4spn_pair_b5_b7: .asciz "C.ADDI4SPN nzuimm bits 5+7 together (=160)"
str_a4spn_pair_b5_b8: .asciz "C.ADDI4SPN nzuimm bits 5+8 together (=288)"
str_a4spn_pair_b5_b9: .asciz "C.ADDI4SPN nzuimm bits 5+9 together (=544)"
str_a4spn_pair_b6_b7: .asciz "C.ADDI4SPN nzuimm bits 6+7 together (=192)"
str_a4spn_pair_b6_b8: .asciz "C.ADDI4SPN nzuimm bits 6+8 together (=320)"
str_a4spn_pair_b6_b9: .asciz "C.ADDI4SPN nzuimm bits 6+9 together (=576)"
str_a4spn_pair_b7_b8: .asciz "C.ADDI4SPN nzuimm bits 7+8 together (=384)"
str_a4spn_pair_b7_b9: .asciz "C.ADDI4SPN nzuimm bits 7+9 together (=640)"
str_a4spn_pair_b8_b9: .asciz "C.ADDI4SPN nzuimm bits 8+9 together (=768)"
str_a4spn_sp_preserved_2: .asciz "C.ADDI4SPN sp unmodified (rd=a3, bits 2+7)"
str_a4spn_sp_preserved_3: .asciz "C.ADDI4SPN sp unmodified (rd=a5, max)"
str_a4spn_sp_preserved_4: .asciz "C.ADDI4SPN sp unmodified (rd=s1, alt pattern)"
str_sw_rs2_s1: .asciz "C.SW rs2=s1(x9)"
str_sw_rs2_a0: .asciz "C.SW rs2=a0(x10)"
str_sw_rs2_a1: .asciz "C.SW rs2=a1(x11)"
str_sw_rs2_a2: .asciz "C.SW rs2=a2(x12)"
str_sw_rs2_a3: .asciz "C.SW rs2=a3(x13)"
str_sw_rs2_a4: .asciz "C.SW rs2=a4(x14)"
str_sw_rs2_a5: .asciz "C.SW rs2=a5(x15)"
str_sw_rs1_s0: .asciz "C.SW rs1=s0(x8)"
str_sw_rs1_s1: .asciz "C.SW rs1=s1(x9)"
str_sw_rs1_a0: .asciz "C.SW rs1=a0(x10)"
str_sw_rs1_a1: .asciz "C.SW rs1=a1(x11)"
str_sw_rs1_a2: .asciz "C.SW rs1=a2(x12)"
str_sw_rs1_a3: .asciz "C.SW rs1=a3(x13)"
str_sw_rs1_a5: .asciz "C.SW rs1=a5(x15)"
str_sw_alias_s0: .asciz "C.SW rs1==rs2==s0(x8) alias"
str_sw_alias_a4: .asciz "C.SW rs1==rs2==a4(x14) alias"
str_sw_off0:   .asciz "C.SW offset=0 (min)"
str_sw_off4:   .asciz "C.SW offset=4 (bit2)"
str_sw_off8:   .asciz "C.SW offset=8 (bit3)"
str_sw_off16:  .asciz "C.SW offset=16 (bit4)"
str_sw_off32:  .asciz "C.SW offset=32 (bit5)"
str_sw_off64:  .asciz "C.SW offset=64 (bit6, max single bit)"
str_sw_off124: .asciz "C.SW offset=124 (max, all bits)"
str_sw_off84:  .asciz "C.SW offset=84 (alt pattern 10101)"
str_sw_off40:  .asciz "C.SW offset=40 (alt pattern 01010)"
str_sw_val_zero:     .asciz "C.SW value=0x00000000"
str_sw_val_allones:  .asciz "C.SW value=0xffffffff"
str_sw_val_maxpos:   .asciz "C.SW value=0x7fffffff"
str_sw_val_minneg:   .asciz "C.SW value=0x80000000"
str_sw_val_alt1:     .asciz "C.SW value=0xaaaaaaaa"
str_sw_val_alt0:     .asciz "C.SW value=0x55555555"
str_sw_val_deadbeef: .asciz "C.SW value=0xdeadbeef"
str_sw_val_one:      .asciz "C.SW value=0x00000001"
str_sw_upper_bits_ignored: .asciz "C.SW ignores upper 32 bits of rs2"
str_sw_adjacent_before: .asciz "C.SW does not touch word before target"
str_sw_adjacent_after:  .asciz "C.SW does not touch word after target"
str_sw_target_correct:  .asciz "C.SW target word correct"
str_sw_rs1_preserved: .asciz "C.SW rs1 unmodified"
str_sw_rs2_preserved: .asciz "C.SW rs2 unmodified"
str_lw_rd_s0:  .asciz "C.LW rd=s0(x8)"
str_lw_rd_s1:  .asciz "C.LW rd=s1(x9)"
str_lw_rd_a0:  .asciz "C.LW rd=a0(x10)"
str_lw_rd_a1:  .asciz "C.LW rd=a1(x11)"
str_lw_rd_a2:  .asciz "C.LW rd=a2(x12)"
str_lw_rd_a3:  .asciz "C.LW rd=a3(x13)"
str_lw_rd_a4:  .asciz "C.LW rd=a4(x14)"
str_lw_rd_a5:  .asciz "C.LW rd=a5(x15)"
str_lw_rs1_s0: .asciz "C.LW rs1=s0(x8)"
str_lw_rs1_s1: .asciz "C.LW rs1=s1(x9)"
str_lw_rs1_a0: .asciz "C.LW rs1=a0(x10)"
str_lw_rs1_a1: .asciz "C.LW rs1=a1(x11)"
str_lw_rs1_a2: .asciz "C.LW rs1=a2(x12)"
str_lw_rs1_a3: .asciz "C.LW rs1=a3(x13)"
str_lw_rs1_a4: .asciz "C.LW rs1=a4(x14) (rd==rs1 alias)"
str_lw_rs1_a5: .asciz "C.LW rs1=a5(x15)"
str_lw_off0:   .asciz "C.LW offset=0 (min)"
str_lw_off4:   .asciz "C.LW offset=4 (bit2)"
str_lw_off8:   .asciz "C.LW offset=8 (bit3)"
str_lw_off16:  .asciz "C.LW offset=16 (bit4)"
str_lw_off32:  .asciz "C.LW offset=32 (bit5)"
str_lw_off64:  .asciz "C.LW offset=64 (bit6, max single bit)"
str_lw_off124: .asciz "C.LW offset=124 (max, all bits)"
str_lw_off84:  .asciz "C.LW offset=84 (alt pattern 10101)"
str_lw_off40:  .asciz "C.LW offset=40 (alt pattern 01010)"
str_lw_val_zero:     .asciz "C.LW value=0x00000000"
str_lw_val_allones:  .asciz "C.LW value=0xffffffff (sign-extends)"
str_lw_val_maxpos:   .asciz "C.LW value=0x7fffffff (no sign-ext)"
str_lw_val_minneg:   .asciz "C.LW value=0x80000000 (sign-extends)"
str_lw_val_alt1:     .asciz "C.LW value=0xaaaaaaaa (sign-extends)"
str_lw_val_alt0:     .asciz "C.LW value=0x55555555 (no sign-ext)"
str_lw_val_deadbeef: .asciz "C.LW value=0xdeadbeef (sign-extends)"
str_lw_val_one:      .asciz "C.LW value=0x00000001"
str_lw_rs1_preserved:  .asciz "C.LW rs1 unmodified"
str_lw_mem_unchanged:  .asciz "C.LW memory unmodified (pure read)"
str_sd_rs2_s1: .asciz "C.SD rs2=s1(x9)"
str_sd_rs2_a0: .asciz "C.SD rs2=a0(x10)"
str_sd_rs2_a1: .asciz "C.SD rs2=a1(x11)"
str_sd_rs2_a2: .asciz "C.SD rs2=a2(x12)"
str_sd_rs2_a3: .asciz "C.SD rs2=a3(x13)"
str_sd_rs2_a4: .asciz "C.SD rs2=a4(x14)"
str_sd_rs2_a5: .asciz "C.SD rs2=a5(x15)"
str_sd_rs1_s0: .asciz "C.SD rs1=s0(x8)"
str_sd_rs1_s1: .asciz "C.SD rs1=s1(x9)"
str_sd_rs1_a0: .asciz "C.SD rs1=a0(x10)"
str_sd_rs1_a1: .asciz "C.SD rs1=a1(x11)"
str_sd_rs1_a2: .asciz "C.SD rs1=a2(x12)"
str_sd_rs1_a3: .asciz "C.SD rs1=a3(x13)"
str_sd_rs1_a5: .asciz "C.SD rs1=a5(x15)"
str_sd_alias_s0: .asciz "C.SD rs1==rs2==s0(x8) alias"
str_sd_alias_a4: .asciz "C.SD rs1==rs2==a4(x14) alias"
str_sd_off0:   .asciz "C.SD offset=0 (min)"
str_sd_off8:   .asciz "C.SD offset=8 (bit3)"
str_sd_off16:  .asciz "C.SD offset=16 (bit4)"
str_sd_off32:  .asciz "C.SD offset=32 (bit5)"
str_sd_off64:  .asciz "C.SD offset=64 (bit6)"
str_sd_off128: .asciz "C.SD offset=128 (bit7, max single bit)"
str_sd_off248: .asciz "C.SD offset=248 (max, all bits)"
str_sd_off168: .asciz "C.SD offset=168 (alt pattern 10101)"
str_sd_off80:  .asciz "C.SD offset=80 (alt pattern 01010)"
str_sd_val_zero:     .asciz "C.SD value=0x0000000000000000"
str_sd_val_allones:  .asciz "C.SD value=0xffffffffffffffff"
str_sd_val_maxpos:   .asciz "C.SD value=0x7fffffffffffffff"
str_sd_val_minneg:   .asciz "C.SD value=0x8000000000000000"
str_sd_val_alt1:     .asciz "C.SD value=0xaaaaaaaaaaaaaaaa"
str_sd_val_alt0:     .asciz "C.SD value=0x5555555555555555"
str_sd_val_walking:  .asciz "C.SD value=0x0123456789abcdef"
str_sd_val_one:      .asciz "C.SD value=0x0000000000000001"
str_sd_adjacent_before: .asciz "C.SD does not touch doubleword before target"
str_sd_adjacent_after:  .asciz "C.SD does not touch doubleword after target"
str_sd_target_correct:  .asciz "C.SD target doubleword correct"
str_sd_rs1_preserved: .asciz "C.SD rs1 unmodified"
str_sd_rs2_preserved: .asciz "C.SD rs2 unmodified"

str_ld_rd_s0:  .asciz "C.LD rd=s0(x8)"
str_ld_rd_s1:  .asciz "C.LD rd=s1(x9)"
str_ld_rd_a0:  .asciz "C.LD rd=a0(x10)"
str_ld_rd_a1:  .asciz "C.LD rd=a1(x11)"
str_ld_rd_a2:  .asciz "C.LD rd=a2(x12)"
str_ld_rd_a3:  .asciz "C.LD rd=a3(x13)"
str_ld_rd_a4:  .asciz "C.LD rd=a4(x14)"
str_ld_rd_a5:  .asciz "C.LD rd=a5(x15)"
str_ld_rs1_s0: .asciz "C.LD rs1=s0(x8)"
str_ld_rs1_s1: .asciz "C.LD rs1=s1(x9)"
str_ld_rs1_a0: .asciz "C.LD rs1=a0(x10)"
str_ld_rs1_a1: .asciz "C.LD rs1=a1(x11)"
str_ld_rs1_a2: .asciz "C.LD rs1=a2(x12)"
str_ld_rs1_a3: .asciz "C.LD rs1=a3(x13)"
str_ld_rs1_a4: .asciz "C.LD rs1=a4(x14) (rd==rs1 alias)"
str_ld_rs1_a5: .asciz "C.LD rs1=a5(x15)"
str_ld_off0:   .asciz "C.LD offset=0 (min)"
str_ld_off8:   .asciz "C.LD offset=8 (bit3)"
str_ld_off16:  .asciz "C.LD offset=16 (bit4)"
str_ld_off32:  .asciz "C.LD offset=32 (bit5)"
str_ld_off64:  .asciz "C.LD offset=64 (bit6)"
str_ld_off128: .asciz "C.LD offset=128 (bit7, max single bit)"
str_ld_off248: .asciz "C.LD offset=248 (max, all bits)"
str_ld_off168: .asciz "C.LD offset=168 (alt pattern 10101)"
str_ld_off80:  .asciz "C.LD offset=80 (alt pattern 01010)"
str_ld_val_zero:     .asciz "C.LD value=0x0000000000000000"
str_ld_val_allones:  .asciz "C.LD value=0xffffffffffffffff"
str_ld_val_maxpos:   .asciz "C.LD value=0x7fffffffffffffff"
str_ld_val_minneg:   .asciz "C.LD value=0x8000000000000000"
str_ld_val_alt1:     .asciz "C.LD value=0xaaaaaaaaaaaaaaaa"
str_ld_val_alt0:     .asciz "C.LD value=0x5555555555555555"
str_ld_val_walking:  .asciz "C.LD value=0x0123456789abcdef"
str_ld_val_one:      .asciz "C.LD value=0x0000000000000001"
str_ld_rs1_preserved:  .asciz "C.LD rs1 unmodified"
str_ld_mem_unchanged:  .asciz "C.LD memory unmodified (pure read)"

/* =======================================================================
 *  Data buffers used by the load/store instruction tests
 * ======================================================================= */
    .section .bss
    .align 3
    .align 3
word_buf:     .space 256      /* room for offsets up to 124 (C.LW/C.SW max) */
dword_buf:    .space 256      /* room for offsets up to 248 (C.LD/C.SD max) */
